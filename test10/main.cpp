// test package & event request ( client )
#include "main.h"

// package struct declare ( same with server )
using Package = xx::net::PackageBase<uint32_t, int32_t, sizeof(uint32_t), false, 512, 0>;

struct NetCtx;
using PeerBase = xx::net::TaskTcpSocket<NetCtx>;    // contain tasks
struct NetCtx : xx::net::NetCtxTaskBase<NetCtx, PeerBase> {};   // contain taskPeers

struct ClientPeer : PeerBase, xx::net::PartialCodes_SendRequest<ClientPeer, Package> {
    int OnConnect() {
        xx::CoutN("fd = ", fd, " OnConnect");
        nc->taskPeers.Add(xx::WeakFromThis(this));
        return 0;
    }
    void Go() { tasks.AddTask(Go_()); }
    xx::Task<> Go_() {
        auto dr = co_await SendRequest([](xx::Data &d) {
            d.Write("hello");
        });
        xx::CoutN("fd = ", fd, " SendRequest return data = ", dr);
        co_yield -1;    // let task executor kill peer
    }
};

int main() {
    NetCtx nc;
    nc.tasks.Add([&]()->xx::Task<> {
        auto a = xx::net::ToAddress("127.0.0.1", 12222);
        while (true) {
            co_yield 0;
            if (auto w = co_await nc.Connect<ClientPeer>(a, 10)) {
                w->Go();
                break;
            }
            xx::CoutN("Connect failed. retry ...");
        }
    });
    while (nc.RunOnce(1)) {
        std::this_thread::sleep_for(0.5s);
    }
    xx::CoutN("end");
    return 0;
}
