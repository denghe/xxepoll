// shared version ( client )

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
        return Send((void*)"a", 1); // repeat send
    }
};

int main() {
    NetCtx nc;

    for (int i = 0; i < 6; ++i) {
        nc.AddCoro([](NetCtx& nc)->xx::Coro{
            sockaddr_in6 addr{};
            xx_assert(-1 != xx::net::FillAddress("127.0.0.1", 12333, addr));
            int r{};
            xx::Weak<ClientPeer> w;
            LabRetry:
            xx::CoutN("********************************************************* begin connect.");
            CoSleep(0.2s);
            CoAwait( nc.Connect(r, w, addr, 3) );
            if (!w) goto LabRetry;     // log r ?
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
