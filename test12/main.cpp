// test split & combine package & event request ( client )
#include "main.h"

// package struct declare
using Package = xx::net::PackageBase<uint32_t, int32_t, sizeof(uint32_t), false, 512, 0>;

struct NetCtx;
struct TaskPeer : xx::net::TcpSocket<NetCtx> {
    xx::EventTasks<> tasks;
};

template<typename Derived>
struct PeerBase : TaskPeer, xx::net::PartialCodes_SendRequest<Derived, Package> {
    int OnConnect() {
        xx::CoutN("fd = ", fd, " OnConnect");
        ((Derived*)this)->nc->taskPeers.Add(xx::WeakFromThis(this));
        return 0;
    }
};

struct NetCtx : xx::net::NetCtxBase<NetCtx> {
    xx::List<xx::Weak<TaskPeer>, int> taskPeers;
    int OnRunOnce() {
        if (auto c = taskPeers.len) {
            for(int i = c - 1; i >= 0; --i) {
                if (auto& w = taskPeers[i]) {
                    if (w->tasks()) {
                                xx_assert(w);
                        SocketDispose(*w);
                        w.Reset();
                        taskPeers.SwapRemoveAt(i);
                    }
                } else {
                    taskPeers.SwapRemoveAt(i);
                }
            }
        }
        return taskPeers.len;
    }
};

/**************************************************************************************************************/
/**************************************************************************************************************/

struct ClientPeer : PeerBase<ClientPeer> {
    void Go() { tasks.AddTask(Go_()); }
    xx::Task<> Go_() {
        auto dr = co_await SendRequest([](xx::Data &d) {
            d.Write("hello");
        });
        xx::CoutN("fd = ", fd, " SendRequest return = ", dr);
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
