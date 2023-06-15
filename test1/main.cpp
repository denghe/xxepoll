#include "main.h"

// tips:
// coro must be a struct's member or "static function"
// owner can't move if captured "this"

template<typename Derived>
struct PeerBase {

    template<typename T>
    void Receive(T&& data) {
        recvs.push(std::forward<T>(data));
        HandleReceive();
    }

protected:
    std::queue<std::string> recvs;

    xx::Coro HandleReceive = HandleReceive_();
    xx::Coro HandleReceive_() {
        co_yield 0;
        while (!recvs.empty()) {
            ((Derived*)this)->ReceivePackage(std::move(recvs.front()));
            recvs.pop();
            co_yield 0;
        }
    }
};

struct Peer : PeerBase<Peer> {

    template<typename T>
    void ReceivePackage(T&& pkg) {
        coros.Add(ReceivePackage_(std::forward<T>(pkg)));
    }

    bool Update() {
        return coros();
    }

protected:
    xx::Coros coros;

    template<typename T>
    xx::Coro ReceivePackage_(T pkg) {
        std::cout << "begin handle pkg = " << pkg << std::endl;
        co_yield 0;
        std::cout << "end handle pkg = " << pkg << std::endl;
    }
};

int main() {
    Peer p;
    for (size_t i = 0; i < 9; i++) {        // epoll wait ?
        p.Receive(std::to_string(i));
    }
    while (p.Update()) {}   // call logic code ?
	return 0;
}
