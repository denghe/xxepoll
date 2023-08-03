// test package & event request ( server )
#include "main.h"

// package struct declare ( same with client )
using Package = xx::net::PackageBase<uint32_t, int32_t, sizeof(uint32_t), false, 512, 0>;

struct NetCtx : xx::net::NetCtxBase<NetCtx> {};
struct ServerPeer : xx::net::TcpSocket<NetCtx>, xx::net::PartialCodes_SendRequest<ServerPeer, Package> {
    int OnAccept() {
        xx::CoutN("fd = ", fd, " OnAccept ip = ", addr);
        return 0;
    }
    int HandleRequest(Package& pkg) {
        return SendResponse(pkg.serial, pkg.data);  // echo back
    }
};

int main() {
    NetCtx nc;
    nc.Listen<ServerPeer>(12222);
    while (true) {
        nc.RunOnce(1);
    }
    return 0;
}
