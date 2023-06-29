// shared version ( server + client )

#include "main.h"

struct NetCtx : NetCtxBase<NetCtx> {
    size_t counter{};
};

struct ServerPeer : TcpSocket<NetCtx> {
    int OnAccept() { xx::CoutN("ServerPeer OnAccept. fd = ", fd," ip = ", addr); return 0; }
    ~ServerPeer() { xx::CoutN("ServerPeer ~ServerPeer. ip = ", addr); }

    xx::Data recv;  // received data container
    int OnEvents(uint32_t e) {
        //xx::CoutN("ServerPeer OnEvents. fd = ", fd," ip = ", addr, ". e = ", e);
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

struct ClientPeer : TcpSocket<NetCtx> {
    ~ClientPeer() { xx::CoutN("ClientPeer ~ClientPeer."); }

    xx::Data recv;  // received data container
    int OnEvents(uint32_t e) {
        //xx::CoutN("ServerPeer OnEvents. fd = ", fd," ip = ", addr, ". e = ", e);
        if (e & EPOLLERR || e & EPOLLHUP) return -888;    // fatal error
        if (e & EPOLLOUT) {
            if (int r = Send()) return r;
        }
        if (e & EPOLLIN) {
            if (int r = ReadData(fd, recv)) return r;
            xx_assert(recv.len == 1);
            if (int r = Send((void*)"a", 1)) return r; // repeat send
        }
        recv.Clear(/*true*/);   // recv.Shrink();
        return 0;
    }
};


int main() {
    NetCtx nc;
    auto fd = nc.Listen<ServerPeer>(12345);
    xx::CoutN("listener 12345 fd = ", fd);
    fd = nc.Listen<ServerPeer>(12333);
    xx::CoutN("listener 12333 fd = ", fd);

    for (int i = 0; i < 2; ++i) {

        nc.coros.Add([](NetCtx& nc)->xx::Coro{
            sockaddr_in6 addr{};
            xx_assert(-1 != FillAddress("127.0.0.1", 12333, addr));
        LabRetry:
            xx::CoutN("********************************************************* begin connect.");
            int r{};
            xx::Weak<ClientPeer> w;
            CoSleep(0.2s);
            CoAwait( nc.Connect(r, w, addr, 3) );
            if (!w) goto LabRetry;     // log r ?
            xx::CoutN("********************************************************* connected.");
        LabLogic:
            if (w) {
                w->Send((void *) "a", 1);   // send first
            }
        }(nc));

    }

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
