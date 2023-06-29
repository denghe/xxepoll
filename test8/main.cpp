// shared version ( server )

#include "main.h"

struct NetCtx : NetCtxBase<NetCtx> {
    int64_t counter{};
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

int main() {
    NetCtx nc;
    auto fd = nc.Listen<ServerPeer>(12345);
    xx::CoutN("listener 12345 fd = ", fd);
    fd = nc.Listen<ServerPeer>(55555);
    xx::CoutN("listener 55555 fd = ", fd);
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
