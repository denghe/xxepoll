// shared version ( client )

#include "main.h"

struct NetCtx : NetCtxBase<NetCtx> {
    int64_t counter{};
};

struct ClientPeer : TcpSocket<NetCtx> {
    ~ClientPeer() { xx::CoutN("ClientPeer ~ClientPeer."); }

    xx::Data recv;  // received data container
    int OnEvents(uint32_t e) {
        //xx::CoutN("ClientPeer OnEvents. fd = ", fd, ". e = ", e);
        if (e & EPOLLERR || e & EPOLLHUP) return -888;    // fatal error
        if (e & EPOLLOUT) {
            if (int r = Send()) return r;
        }
        if (e & EPOLLIN) {
            if (int r = ReadData(fd, recv)) return r;
            nc->counter++;
            if (int r = Send(recv.buf, recv.len)) return r; // echo back
        }
        recv.Clear(/*true*/);   // recv.Shrink();
        return 0;
    }
};

int main() {
    NetCtx nc;

    for (int i = 0; i < 6; ++i) {
        nc.coros.Add([](NetCtx& nc)->xx::Coro{
            sockaddr_in6 addr{};
            xx_assert(-1 != FillAddress("127.0.0.1", 55555, addr));
        LabRetry:
            xx::CoutN("begin connect.");
            int r{};
            xx::Weak<ClientPeer> w;
            CoSleep(0.2s);
            CoAwait( nc.Connect(r, w, addr, 3) );
            if (!w) goto LabRetry;     // log r ?
            xx::CoutN("connected.");
        LabLogic:
            if (w) {
                w->Send((void *) "a", 1);
            }
        }(nc));
    }

    auto secs = xx::NowSteadyEpochSeconds();
    double timePool{};
    while(true) {
        nc.Wait(1);
        nc.coros();
        if (timePool += xx::NowSteadyEpochSeconds(secs); timePool > 1.) {
            timePool -= 1;
            xx::CoutN("counter = ", nc.counter);
            nc.counter = 0;
        }
    }
    return 0;
}
