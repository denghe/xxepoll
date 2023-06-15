#include "main.h"

struct EchoPeer : TcpPeerBase<EchoPeer> {
    int Receive() {
        Send(xx::Data(recv));
        recv.Clear();
        return 0;
    }
};

int main() {
    Epoll ep;
    TcpListenerBase<EchoPeer> listener;
    if (int r = listener.Init(&ep, ep.MakeSocket(12345))) return r;
    xx::CoutN("running...");
    while(ep.running) {
        if (int r = ep.Wait(1)) return r;
    }
    return 0;
}
