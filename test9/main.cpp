// test split & combine package
#include "main.h"
struct NetCtx : xx::net::NetCtxBase<NetCtx> {};

// package define: header len = 1 byte, max = 256
template<typename Derived>
struct PeerBase : xx::net::TcpSocket<NetCtx>, xx::net::PartialCodes_OnEvents_Pkg<Derived, uint8_t, 1, false, 256, 255> {};

struct ServerPeer : PeerBase<ServerPeer> {
    int OnEventsPkg(xx::Data_r dr) {
        return Send(dr);   // echo
    }
};

struct ClientPeer : PeerBase<ClientPeer> {
    int OnEventsPkg(xx::Data_r dr) {
        xx::CoutN("recv dr = ", dr);
        return 1;   // close
    }
    void BeginLogic() { AddCondCoroToNC(BeginLogic_()); }
    xx::Coro BeginLogic_() {
        auto d = xx::Data::From({3, 1, 2, 3});
        for(size_t i = 0; i < d.len; ++i) {
            Send(&d[i], 1);
            xx::CoutN("i = ", i);
            CoYield;
        }
        xx::CoutN("send finished.");
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
