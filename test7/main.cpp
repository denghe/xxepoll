// simple echo client with performance test

#include "main.h"

struct NetCtx : xx::net::NetCtxBase<NetCtx> {
    size_t counter{};
};

template<typename Derived>
struct PeerBase : xx::net::TcpSocket<NetCtx>, xx::net::PartialCodes_OnEvents_Base<Derived, 1024> {};

struct ClientPeer : PeerBase<ClientPeer> {
    ~ClientPeer() { xx::CoutN("ClientPeer ~ClientPeer."); }

    int OnEventsIn() {
        xx_assert(recv.len == 1);
        recv.Clear();
        ++nc->counter;
        return Send((void*)"a", 1); // repeat send
    }
};

int main() {
    NetCtx nc;

    for (int i = 0; i < 6; ++i) {
        nc.tasks.AddTask([](NetCtx& nc)->xx::Task<> {
            sockaddr_in6 addr{};
            xx_assert(-1 != xx::net::FillAddress("127.0.0.1", 55555, addr));
        LabRetry:
            xx::CoutN("********************************************************* begin connect.");
            co_yield 0;
            co_yield 0;
            auto w = co_await nc.Connect<ClientPeer>(addr, 3);
            if (!w) goto LabRetry;
            xx::CoutN("********************************************************* connected.");
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
