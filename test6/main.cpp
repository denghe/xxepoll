﻿// shared version ( server + client )

#include "main.h"

struct NetCtx : xx::net::NetCtxBase<NetCtx> {
    size_t counter{};
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

struct ClientPeer : PeerBase<ClientPeer> {
    ~ClientPeer() { xx::CoutN("ClientPeer ~ClientPeer."); }

    int OnEventsIn() {
        xx_assert(recv.len == 1);
        recv.Clear();
        return Send((void*)"a", 1); // repeat send
    }
};

int main() {
    NetCtx nc;
    nc.Listen<ServerPeer>(12333);

    for (int i = 0; i < 2; ++i) {
        nc.tasks.Add([](NetCtx& nc)->xx::Task<> {
            sockaddr_in6 addr{};
            xx_assert(-1 != xx::net::FillAddress("127.0.0.1", 12333, addr));
        LabRetry:
            xx::CoutN("********************************************************* begin connect.");
            co_yield 0;
            co_yield 0;
            auto [r, w] = co_await nc.Connect<ClientPeer>(addr, 3);
            if (!r) goto LabRetry;     // log r ?
            xx::CoutN("********************************************************* connected.");
            xx_assert(w);
            w->Send((void *) "a", 1);   // send first data
        }(nc));
    }

    double secs = xx::NowSteadyEpochSeconds(), timePool{};
    while (nc.RunOnce(1)) {
        if (timePool += xx::NowSteadyEpochSeconds(secs); timePool > 1.) {
            timePool -= 1;
            xx::CoutN("counter = ", nc.counter);
            nc.counter = 0;
        }
    }
    return 0;
}
