// weak version

#include "main.h"

namespace xx {
    template<>
    struct StringFuncs<sockaddr const *, void> {
        static inline void Append(std::string &s, sockaddr const *const &in) {
            if (in) {
                char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
                if (!getnameinfo(in, in->sa_family == AF_INET6 ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN, hbuf, sizeof hbuf,
                                 sbuf, sizeof sbuf, NI_NUMERICHOST | NI_NUMERICSERV)) {
                    s.append(hbuf);
                    s.push_back(':');
                    s.append(sbuf);
                }
            }
        }
    };

    template<>
    struct StringFuncs<sockaddr_in6, void> {
        static inline void Append(std::string &s, sockaddr_in6 const &in) {
            StringFuncs<sockaddr const *>::Append(s, (sockaddr const *) &in);
        }
    };
}

void CloseFD(int fd) {
    for (int r = close(fd); r == -1 && errno == EINTR;);
}

// for create listener or client socket fd
template<bool enableReuseAddr = true>
[[nodiscard]] int MakeSocketFD(int port, int sockType = SOCK_STREAM, char const* hostName = {}) {
    int fd;
    addrinfo hints{};
    bzero(&hints, sizeof(addrinfo));
    hints.ai_family = AF_UNSPEC;  // ipv4 / 6
    hints.ai_socktype = sockType; // SOCK_STREAM / SOCK_DGRAM
    hints.ai_flags = AI_PASSIVE;  // all interfaces
    addrinfo *ai_{}, *ai;
    if (getaddrinfo(hostName, std::to_string(port).c_str(), &hints, &ai_)) return -1; // format error?
    for (ai = ai_; ai != nullptr; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
        if (fd == -1) continue;
        if constexpr(enableReuseAddr) {
            int enable = 1;
            if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                close(fd);
                continue;
            }
        }
        if (!bind(fd, ai->ai_addr, ai->ai_addrlen)) break; // success
        close(fd);
    }
    freeaddrinfo(ai_);
    if (!ai) return -2; // address not found?
    return fd;
}

// for listener accept
[[nodiscard]] int MakeAcceptFD(int listenerFD, sockaddr_in6* addr) {
    socklen_t len = sizeof(sockaddr_in6);
    if (int fd = accept4(listenerFD, (sockaddr *)addr, &len, SOCK_NONBLOCK); fd == -1) {
        if (auto e = errno; e == EAGAIN) return 0;  // other process handled?
        else return -e;
    } else return fd;
}

// for client socket
[[nodiscard]] int SetNonBlock(int fd) {
    int r = fcntl(fd, F_GETFL, 0);
    if (r == -1) return -errno;
    r = fcntl(fd, F_SETFL, r | O_NONBLOCK);
    if (r == -1) return -errno;
    return 0;
}

// read all data from fd. append to d
// success: return 0
// < 0 == -errno, need close fd
template<size_t reserveLen = 1024 * 256>
[[nodiscard]] int ReadData(int fd, xx::Data& d) {
LabBegin:
    d.Reserve(reserveLen);
    auto span = d.GetFreeSpace();
    assert(span.len);
LabRepeat:
    if (auto n = ::read(fd, span.buf, span.len); n > 0) {
        d.len += n;
        if (n == span.len) goto LabBegin;
        return 0;
    } else if (n == 0) return 0;
    else if (auto e = errno; e == EAGAIN || e == EWOULDBLOCK) goto LabRepeat;
    else return -e;
}

// write data to fd
// success: return "the number of bytes written" + blocked
// < 0 == -errno, need close fd
[[nodiscard]] std::pair<ssize_t, bool> WriteData(int fd, void* buf_, size_t len_) {
    auto buf = (char*)buf_;
    auto len = len_;
LabRepeat:
    if (auto n = ::write(fd, buf, len); n > 0) {
        if (len -= n; len > 0) {
            buf += n;
            goto LabRepeat;
        } else return {(ssize_t)len_, false };
    } else {
        if (auto e = errno; e == EAGAIN || e == EWOULDBLOCK) return {(char*)buf_ - buf + n, true };
        else return {-e, false };
    }
}

struct FdBase {
    int fd{ -1 };
    mutable int errorNumber{};

    // epoll events handler. return non 0: CloseFD + CtlDel
    // example:  = [](FdBase* s, uint32_t e) { return ((CLASSNAME*)s)->OnEvents_(e); };
    int (*OnEvents)(FdBase* s, uint32_t e){} ;

    FdBase() = default;
    FdBase(FdBase const&) = delete;
    FdBase& operator=(FdBase const&) = delete;
    ~FdBase() {
        xx_assert(fd == -1);    //FdDispose();
    }
    void FdDispose() {
        if (fd != -1) {
            CloseFD(fd);
            fd = -1;
        }
    }
};

struct Epoll : FdBase {
    using FdBase::FdBase;
    int Init() {
        return (fd = epoll_create(1));
    }

    // op == EPOLL_CTL_ADD, EPOLL_CTL_MOD
    // events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLET
    template<int op = EPOLL_CTL_ADD, uint32_t events = EPOLLIN | EPOLLOUT | EPOLLET>
    [[nodiscard]] int Ctl(int socketFD, void* ptr) const {
        epoll_event ee {events, ptr};
        if (int r = epoll_ctl(fd, op, socketFD, &ee); r == -1) return -errno;
        return 0;
    }

    // op == EPOLL_CTL_DEL
    [[nodiscard]] int CtlDel(int socketFD) const {
        if (int r = epoll_ctl(fd, EPOLL_CTL_DEL, socketFD, nullptr); r == -1) return -errno;
        return 0;
    }
};

/*******************************************************************************************************/
/*******************************************************************************************************/


template<typename T> concept Has_OnAccept = requires(T t) { t.OnAccept(); };
template<typename T> concept Has_OnReceive = requires(T t) { t.OnReceive(); };
template<typename T> concept Has_OnClose = requires(T t) { t.OnClose(); };

using IdxVerType = xx::ListDoubleLinkIndexAndVersion<int, uint>;    // int + uint  for store to  epoll int64 user data

template<typename NetCtxType>
struct Socket : FdBase {
    using FdBase::FdBase;

    NetCtxType* nc{};
    IdxVerType iv;
    sockaddr_in6 addr{};

    void Fill(int fd_, NetCtxType* nc_, IdxVerType iv_, sockaddr_in6 const* addr_ = {}) {
        assert(fd == -1);
        assert(!nc);
        assert(iv.index == -1 && iv.version == 0);
        fd = fd_;
        nc = nc_;
        iv = iv_;
        if (addr_) {
            addr = *addr_;
        } else {
            bzero(&addr, sizeof(addr));
        }
    }

};

template<typename NetCtxType, typename PeerType>
struct ListenerBase : Socket<NetCtxType> {
    using Socket<NetCtxType>::Socket;

    ~ListenerBase() {
        this->FdDispose();
    }
    void Init() {
        this->OnEvents = [](FdBase* s, uint32_t e) { return ((ListenerBase<NetCtxType, PeerType>*)s)->OnEvents_(e); };
    }
    int OnEvents_(uint32_t e);
};

template<typename Derived>
struct NetCtxBase : Epoll {
    using SocketType = Socket<NetCtxBase>;
    using Epoll::Epoll;

    std::array<epoll_event, 4096> events{}; // tmp epoll events container
    xx::ListDoubleLink<xx::Weak<Socket<Derived>>, int, uint> sockets;    // listeners + accepted peers container
    IdxVerType lastListenerIV;  // for visit all sockets, skip listeners ( Foreach( []{}, Next( iv ) )

    ~NetCtxBase() {
        FdDispose();
    }

    template<typename ListenerType, class = std::enable_if_t<std::is_base_of_v<Socket<Derived>, ListenerType>>>
    xx::Shared<ListenerType> Listen(int port, int sockType = SOCK_STREAM, char const* hostName = {}) {
        int r = MakeSocketFD(port, sockType, hostName);
        if (r < 0) return {};
        auto sg = xx::MakeScopeGuard([&]{
            CloseFD(r);
        });
        if (int n = listen(r, SOMAXCONN); n == -1) {
            // log errno?
            return {};
        }

        auto& w = sockets.Emplace();    // weak ptr container
        auto iv = sockets.Tail();
        if (int n = Ctl(r, (void*&)iv); n < 0) {
            sockets.Remove(iv);
            // log errno?
            return {};
        }
        sg.Cancel();
        lastListenerIV = iv;
        auto L = xx::Make<ListenerType>();
        L->Fill(r, (Derived*)this, iv);
        L->Init();
        w = L;
        return L;
    }

    template<typename PeerType>
    int MakeAcceptPeer(int fd_, sockaddr_in6 const& addr) {
        auto& w = sockets.Emplace();    // weak ptr container
        auto iv = sockets.Tail();
        auto sg = xx::MakeScopeGuard([&]{ sockets.Remove(iv); });
        if (int n = Ctl(fd_, (void*&)iv); n < 0) return n;   // system error?
        auto s = xx::Make<PeerType>();
        s->Fill(fd_, (Derived*)this, iv, &addr);
        if constexpr (Has_OnAccept<PeerType>) {
            if (s->OnAccept()) return 0;
        }
        w = s;
        sg.Cancel();
        return 0;
    }

    int Wait(int timeoutMS) {
        xx_assert(fd != -1);    // forget Init ?
        int n = epoll_wait(fd, events.data(), (int) events.size(), timeoutMS);
        if (n == -1) return -errno;
        for (int i = 0; i < n; ++i) {
            auto& iv = (IdxVerType&)events[i].data.ptr;
            xx_assert(sockets.Exists(iv));
            auto& w = sockets[iv];
            xx_assert(w);
            auto bak = w->fd;
            if (int r = w->OnEvents(w.pointer(), events[i].events)) {
                if (-1 == CtlDel(bak)) return -errno;
                CloseFD(bak);
                sockets.Remove(iv);
            }
        }
        return 0;
    }
};

template<typename NetCtxType, typename PeerType>
int ListenerBase<NetCtxType, PeerType>::OnEvents_(uint32_t e) {
    assert(!(e & EPOLLERR || e & EPOLLHUP));
    auto HandleEvent = [this] {
        sockaddr_in6 addr{};
        int s = MakeAcceptFD(this->fd, &addr);
        if (s == 0) return 0;   // no more accept?
        else if (s < 0) return -errno;   // system error?
        this->nc->template MakeAcceptPeer<PeerType>(s, addr);
        return 0;
    };
LabRepeat:
    int r = HandleEvent();
    if (r == 0) goto LabRepeat;
    else return r;
}

/*******************************************************************************************************/
/*******************************************************************************************************/

struct NetCtx : NetCtxBase<NetCtx> {
    using NetCtxBase<NetCtx>::NetCtxBase;
};

struct Peer : Socket<NetCtx> {
    int OnAccept() {
        this->OnEvents = [](FdBase* s, uint32_t e) { return ((Peer*)s)->OnEvents_(e); };
        // todo: store peer to netctx
        xx::CoutN("peer accepted. ip = ", addr);
        return 0;
    }
    int OnEvents_(uint32_t e) {
        // todo: echo
        xx::CoutN("peer events = ", e, ". ip = ", addr);
        return 0;
    }
};

struct Listener : ListenerBase<NetCtx, Peer> {
    using ListenerBase<NetCtx, Peer>::ListenerBase;
};

int main() {
    NetCtx nc;
    xx_assert(nc.Init() >= 0);
    auto L1 = nc.Listen<Listener>(12345);
    xx_assert(L1);
    auto L2 = nc.Listen<Listener>(12333);
    xx_assert(L2);
    int r = 0;
    do {
        r = nc.Wait(1);
    } while(r == 0);
    return r;
}
