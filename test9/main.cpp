// test split & combine package
#include "main.h"
struct NetCtx : NetCtxBase<NetCtx> {};

// package define: header len = 1 byte, max = 256
template<typename Derived>
using OnEventsPkg = PartialCodes_OnEventsPkg<Derived, uint8_t, 1, false, 256, 255>;

struct ServerPeer : TcpSocket<NetCtx>, OnEventsPkg<ServerPeer> {
    int OnEventsPkg(xx::Data_r dr) {
        Send(dr);   // echo
        return 0;
    }
};

struct ClientPeer : TcpSocket<NetCtx>, OnEventsPkg<ClientPeer> {
    int OnEventsPkg(xx::Data_r dr) {
        xx::CoutN("recv dr = ", dr);
        return 1;   // close
    }
    void BeginLogic() {
        Go(BeginLogic_());
    }
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
    nc.Go([](NetCtx& nc)->xx::Coro{
        int r{};
        xx::Weak<ClientPeer> w;
        while(true) {
            CoSleep(0.2s);
            CoAwait(nc.Connect(r, w, ToAddress("127.0.0.1", 12222), 3));
            if (w) {
                w->BeginLogic();
                CoReturn;
            }
        }
    }(nc));
    while(nc.RunOnce(1) > 1) {
        std::this_thread::sleep_for(1s);
    }
    return 0;
}
