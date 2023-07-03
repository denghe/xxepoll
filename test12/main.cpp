// test split & combine package
#include "main.h"

/**************************************************************************************************************/

using PkgLen_t = uint32_t;
using PkgSerial_t = int32_t;
struct Package {
    PkgLen_t pkgLen{};
    PkgSerial_t serial{};   // negative: request       0: push        positive: response
    xx::Data_r data;
    int Fill(xx::Data_r& dr) {
        if (int r = dr.ReadFixed(pkgLen)) return r; // fixed len
        if (int r = dr.Read(serial)) return r; // var len
        return dr.ReadLeftBuf(data);
    }
};
using ECoros = xx::EventCoros<PkgSerial_t, xx::Data_r>;
using ECoro = ECoros::CoroType;
using EArgs = ECoros::Args;

template<typename T> concept Has_HandleRequest = requires(T t) { t.HandleRequest(std::declval<Package&>()); };
template<typename T> concept Has_HandlePush = requires(T t) { t.HandlePush(std::declval<Package&>()); };

/**************************************************************************************************************/

struct NetCtx : xx::net::NetCtxBase<NetCtx> {};

// 4 byte len, does not contain self
template<typename Derived>
struct PeerBase : xx::net::TcpSocket<NetCtx>, xx::net::PartialCodes_OnEvents_Pkg<Derived, PkgLen_t, sizeof(PkgLen_t), false> {

    ECoros responseHandlers;
    int32_t autoIncSerial = 0;

    int32_t GenSerial() {
        autoIncSerial = (autoIncSerial + 1) & 0x7FFFFFFF;
        return autoIncSerial;
    }

    void AddECoro(ECoro&& c) {
        responseHandlers.Add(std::move(c));
    }

    template<typename DataFiller>
    int SendResponse(PkgSerial_t serial, DataFiller&& filler) {
        xx::Data d(this->GetReserveLen());
        d.Clear();
        d.WriteJump(sizeof(PkgLen_t));
        d.Write(serial);
        filler(d);
        d.WriteFixedAt(0, PkgLen_t(d.len - sizeof(PkgLen_t))); // len does not contain self
        xx::CoutN("Send d = ", d);
        return Send(std::move(d));
    }

    template<typename DataFiller>
    int SendPush(DataFiller&& filler) {
        return SendResponse(0, std::forward<DataFiller>(filler));
    }

    /* example:
    auto [e, serial] = SendRequest( [...](xx::Data& d) { ... d.Write( ... ); } );
    if (e) { nc->SocketDispose(*this); co_return; }
    xx::Data_r dr;
    co_yield EArgs{ serial, dr };
    */
    template<typename DataFiller>
    std::pair<int, PkgSerial_t> SendRequest(DataFiller&& filler) {
        auto serial = GenSerial();
        return { SendResponse(-serial, std::forward<DataFiller>(filler)), serial };
    }

    int OnAccept() {
        xx::CoutN("OnAccept fd = ", fd, " ip = ", addr);
        AddCondCoroToNC(RegisterUpdateCoro());
        return 0;
    }

    xx::Coro RegisterUpdateCoro() {
        while(true) {
            CoYield;
            responseHandlers.Update();
        }
    }

    int OnEventsPkg(xx::Data_r dr) {
        Package pkg;
        if (int r = pkg.Fill(dr)) return r;
        if constexpr(Has_HandleRequest<Derived>) {
            if (pkg.serial < 0) {
                pkg.serial = -pkg.serial;
                return ((Derived *) this)->HandleRequest(pkg);
            }
        }
        if constexpr(Has_HandlePush<Derived>) {
            if (pkg.serial == 0) return ((Derived*)this)->HandlePush(pkg);
        }
        // handle response
        if (auto r = responseHandlers.Trigger(pkg.serial, pkg.data); !r.has_value()) return 0;
        else return *r;
    }

};

/**************************************************************************************************************/

struct ServerPeer : PeerBase<ServerPeer> {
    int HandleRequest(Package& pkg) {
        std::string msg;
        if (int r = pkg.data.Read(msg)) return r;
        if (msg == "hello") {
            SendResponse(pkg.serial, [&](xx::Data& d){
                d.Write("world");
            });
        }
        return 0;
    }
};

struct ClientPeer : PeerBase<ClientPeer> {
    void BeginLogic() { AddECoro(BeginLogic_()); }
    ECoro BeginLogic_() {
        auto [e, serial] = SendRequest(  [](xx::Data& d) {
            d.Write("hello");
        } );
        if (e) { nc->SocketDispose(*this); co_return; }
        xx::Data_r dr;
        co_yield EArgs{ serial, dr };
        xx::CoutN(dr);
    }
};

int main() {
    NetCtx nc;
    nc.Listen<ServerPeer>(12222);
    nc.AddCoro([](NetCtx& nc)->xx::Coro{
        int r{};
        xx::Weak<ClientPeer> w;
        while (!w) {
            CoSleep(0.2s);
            CoAwait(nc.Connect(r, w, xx::net::ToAddress("127.0.0.1", 12222), 3));
        }
        w->BeginLogic();
    }(nc));
    while(nc.RunOnce(1) > 1) {
        std::this_thread::sleep_for(0.5s);
    }
    return 0;
}
