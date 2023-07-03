// test split & combine package
#include "main.h"

// package define: 4 byte len + serial + typeId + data
struct Package {
    uint32_t pkgLen{};
    int32_t serial{};   // negative: request       0: push        positive: response
    uint32_t typeId{};  // for switch case
    xx::Data_r data;
    int Fill(xx::Data_r& dr) {
        if (int r = dr.ReadFixed(pkgLen)) return r; // fixed len
        if (int r = dr.Read(serial, typeId)) return r; // var
        return dr.ReadLeftBuf(data);
    }
};

// key: serial
using ECoros = xx::EventCoros<int32_t, xx::Data_r>;
using ECoro = ECoros::CoroType;
using EArgs = ECoros::Args;

struct NetCtx : xx::net::NetCtxBase<NetCtx> {};

template<typename Derived>
struct PeerBase : xx::net::TcpSocket<NetCtx>, xx::net::PartialCodes_OnEvents_Pkg<Derived> {
    ECoros responseHandlers;
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
        if (pkg.serial < 0) return ((Derived*)this)->HandleRequest(pkg);
        else if (pkg.serial == 0) return ((Derived*)this)->HandlePush(pkg);
        else return ((Derived*)this)->HandleResponse(pkg);
    }
    int HandleResponse(Package const& pkg) {
        if (auto r = responseHandlers.Trigger(pkg.serial, pkg.data); !r.has_value()) return 0;
        else return *r;
    }
};

struct ServerPeer : PeerBase<ServerPeer> {
    int HandleRequest(Package const& pkg) {
        // todo: switch case pkg.typeId
        return 0;
    }
    int HandlePush(Package const& pkg) {
        // todo: switch case pkg.typeId
        return 0;
    }
};

struct ClientPeer : PeerBase<ClientPeer> {
    int HandleRequest(Package const& pkg) {
        // todo: switch case pkg.typeId
        return 0;
    }
    int HandlePush(Package const& pkg) {
        // todo: switch case pkg.typeId
        return 0;
    }
//    void BeginLogic() { AddCondCoroToNC(BeginLogic_()); }
//    xx::Coro BeginLogic_() {
//        auto d = xx::Data::From({3, 1, 2, 3});
//        for(size_t i = 0; i < d.len; ++i) {
//            Send(&d[i], 1);
//            xx::CoutN("i = ", i);
//            CoYield;
//        }
//        xx::CoutN("send finished.");
//    }
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
        //w->BeginLogic();
    }(nc));
    while(nc.RunOnce(1) > 1) {
        std::this_thread::sleep_for(0.5s);
    }
    return 0;
}
