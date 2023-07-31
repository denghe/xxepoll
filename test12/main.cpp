// test split & combine package & event request
#include "main.h"

/**************************************************************************************************************/

using PkgLen_t = uint32_t;
using PkgSerial_t = int32_t;
struct Package {
    PkgLen_t pkgLen{};
    PkgSerial_t serial{};   // negative: request       0: push        positive: response
    xx::Data_r data;
    int ReadFrom(xx::Data_r& dr) {
        if (int r = dr.ReadFixed(pkgLen)) return r; // fixed len
        if (int r = dr.Read(serial)) return r; // var len
        return dr.ReadLeftBuf(data);
    }
};

template<typename T> concept Has_HandleRequest = requires(T t) { t.HandleRequest(std::declval<Package&>()); };
template<typename T> concept Has_HandlePush = requires(T t) { t.HandlePush(std::declval<Package&>()); };

/**************************************************************************************************************/

struct NetCtx : xx::net::NetCtxBase<NetCtx> {};

// 4 byte len, does not contain self
template<typename Derived>
struct PeerBase : xx::net::TcpSocket<NetCtx>, xx::net::PartialCodes_OnEvents_Pkg<Derived, PkgLen_t, sizeof(PkgLen_t), false> {

    int32_t autoIncSerial = 0;
    int32_t GenSerial() {
        autoIncSerial = (autoIncSerial + 1) & 0x7FFFFFFF;
        return autoIncSerial;
    }

    xx::EventTasks<> tasks;

    template<typename DataFiller>
    int SendResponse(PkgSerial_t serial, DataFiller&& filler) {
        xx::Data d(this->GetReserveLen());
        d.Clear();
        d.WriteJump(sizeof(PkgLen_t));
        d.Write(serial);
        filler(d);
        d.WriteFixedAt(0, PkgLen_t(d.len - sizeof(PkgLen_t))); // len does not contain self
        xx::CoutN("fd = ", fd, " Send d = ", d);
        return Send(std::move(d));
    }

    template<typename DataFiller>
    int SendPush(DataFiller&& filler) {
        return SendResponse(0, std::forward<DataFiller>(filler));
    }

    template<typename DataFiller>
    xx::Task<xx::Data_r> SendRequest(DataFiller&& filler) {
        auto serial = GenSerial();
        if (int r = SendResponse(-serial, std::forward<DataFiller>(filler))) co_yield r;    // let tasks return r
        xx::Data_r d;
        co_yield { serial, &d };
        co_return d;
    }

    int OnAccept() {
        xx::CoutN("fd = ", fd, " OnAccept ip = ", addr);
        AddCondTaskToNC(RegisterUpdateTask());
        return 0;
    }

    xx::Task<> RegisterUpdateTask() {
        while(!tasks()) {
            co_yield 0;
        }
    }

    int OnEventsPkg(xx::Data_r dr) {
        Package pkg;
        if (int r = pkg.ReadFrom(dr)) return r;
        if constexpr(Has_HandleRequest<Derived>) {
            if (pkg.serial < 0) {
                pkg.serial = -pkg.serial;
                return ((Derived *) this)->HandleRequest(pkg);  //
            }
        }
        if constexpr(Has_HandlePush<Derived>) {
            if (pkg.serial == 0) return ((Derived*)this)->HandlePush(pkg);  //
        }
        return tasks(pkg.serial, [&pkg](auto p){    // HandleResponse
            *(xx::Data_r*)p = pkg.data;
        });
    }
};

/**************************************************************************************************************/

struct ClientPeer : PeerBase<ClientPeer> {};

int main() {
    NetCtx nc;
    nc.tasks.AddLambda([&]()->xx::Task<> {
        auto a = xx::net::ToAddress("127.0.0.1", 12222);
        while (true) {
            co_yield 0;
            if (auto w = co_await nc.Connect<ClientPeer>(a, 10)) {
                auto p = &*w;
                xx::CoutN("fd = ", p->fd, " Connected");
                nc.tasks.AddLambda(w, [&nc, p]() -> xx::Task<> {
                    while (!p->tasks()) co_yield 0;      // task executor
                    nc.SocketDispose(*p);   // kill peer
                });
                p->tasks.AddLambda([&nc, p]() -> xx::Task<> {
                    auto dr = co_await p->SendRequest([](xx::Data &d) {
                        d.Write("hello");
                    });
                    xx::CoutN("fd = ", p->fd, " SendRequest( hello ) return dr = ", dr);
                    co_yield -1;    // let task executor kill peer
                });
                break;
            } else {
                xx::CoutN("Connect failed. retry ...");
            }
        }
    });
    while(nc.RunOnce(1)) {
        std::this_thread::sleep_for(0.5s);
    }
    xx::CoutN("end");
    return 0;
}
