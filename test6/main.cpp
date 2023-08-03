// simple echo server with performance test

#include "main.h"

struct NetCtx : xx::net::NetCtxBase<NetCtx> {
    int64_t counter{};
};

template<typename Derived>
struct PeerBase : xx::net::TcpSocket<NetCtx>, xx::net::PartialCodes_OnEvents_Base<Derived, 1024> {};

struct ServerPeer : PeerBase<ServerPeer> {
    ~ServerPeer() { xx::CoutN("ServerPeer ~ServerPeer. ip = ", addr); }

    int OnAccept() { xx::CoutN("ServerPeer OnAccept. fd = ", fd," ip = ", addr); return 0; }
    int OnEventsIn() {
        ++nc->counter;
        if (int r = Send(recv.buf, recv.len)) return r; // echo back
        recv.Clear();
        return 0;
    }
};

int main() {
    NetCtx nc;
    auto fd = nc.Listen<ServerPeer>(12345);
    xx::CoutN("listener 12345 fd = ", fd);
    fd = nc.Listen<ServerPeer>(55555);
    xx::CoutN("listener 55555 fd = ", fd);
    auto secs = xx::NowSteadyEpochSeconds();
    double timePool{};
    while(nc.RunOnce(1)) {
        if (timePool += xx::NowSteadyEpochSeconds(secs); timePool > 1.) {
            timePool -= 1;
            xx::CoutN("counter = ", nc.counter);
            nc.counter = 0;
        }
    }
    return 0;
}
