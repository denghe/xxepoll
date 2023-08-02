// test split & combine package & event request ( server )
#include "main.h"

struct NetCtx : xx::net::NetCtxBase<NetCtx> {};

using Package = xx::net::PackageBase<uint32_t, int32_t, sizeof(uint32_t), false, 512, 0>;

struct ServerPeer : xx::net::TcpSocket<NetCtx>, xx::net::PartialCodes_SendRequest<ServerPeer, Package> {
    int OnAccept() {
        xx::CoutN("fd = ", fd, " OnAccept ip = ", addr);
        return 0;
    }
    int HandleRequest(Package& pkg) {
        std::string msg;
        if (int r = pkg.data.Read(msg)) return r;
        if (msg == "hello") {
            SendResponse(pkg.serial, [&](xx::Data& d){
                d.Write("world");
            });
        }
        return 0;
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
