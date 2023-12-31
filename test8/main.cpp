﻿// test split & combine package
#include "main.h"
struct NetCtx : xx::net::NetCtxBase<NetCtx> {};

// package define: header len = 1 byte, max = 256
template<typename Derived>
struct PeerBase : xx::net::TcpSocket<NetCtx>, xx::net::PartialCodes_OnEvents_Pkg<Derived, uint8_t, 1, false, 256, 255> {};

struct ServerPeer : PeerBase<ServerPeer> {
    int OnEventsPkg(xx::Data_r dr) {
        return Send(dr);   // echo
    }
};

struct ClientPeer : PeerBase<ClientPeer> {
    int OnEventsPkg(xx::Data_r dr) {
        xx::CoutTN("recv dr = ", dr);
        return 1;   // close
    }
    xx::Task<> Go() {
        auto d = xx::Data::From({3, 1, 2, 3});
        for(size_t i = 0; i < d.len; ++i) {
            Send(&d[i], 1);
            xx::CoutTN("i = ", i);
            co_yield 0;
        }
        xx::CoutTN("send finished.");
    }
};

int main() {
    NetCtx nc;
    nc.Listen<ServerPeer>(12222);
    nc.tasks.Add([&]()->xx::Task<> {
        auto a = xx::net::ToAddress("127.0.0.1", 12222);
        while (true) {
            co_yield 0;
            if (auto w = co_await nc.Connect<ClientPeer>(a, 3)) {
                nc.weakTasks.Add(w, w->Go());
                break;
            }
        }
    });
    while(nc.RunOnce(1) > 1) {
        std::this_thread::sleep_for(0.5s);
    }
    return 0;
}
