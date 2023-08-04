// test package & event request ( client )
#include "main.h"

// package struct declare ( same with server )
using Package = xx::net::PackageBase<uint32_t, int32_t, sizeof(uint32_t), false, 512, 0>;

struct NetCtx : xx::net::NetCtxBase<NetCtx> {};
struct ClientPeer : xx::net::TaskTcpSocket<NetCtx>, xx::net::PartialCodes_SendRequest<ClientPeer, Package> {
    xx::Task<> Go() {
        auto dr = co_await SendRequest( xx::Data::From("asdf"));
        xx::CoutN("fd = ", fd, " SendRequest return data = ", dr);
        co_yield -1; // dispose
    }
};

int main() {
    NetCtx nc;
    nc.tasks.Add([&]()->xx::Task<> {
        auto addr = xx::net::ToAddress("127.0.0.1", 12222);
        while (true) {
            co_yield 0;
            if (auto w = co_await nc.Connect<ClientPeer>(addr, 10)) {
                w->tasks.Add(w->Go());
                break;
            }
            xx::CoutN("Connect failed. retry ...");
        }
    });
    nc.Run();
    xx::CoutN("end");
    return 0;
}
