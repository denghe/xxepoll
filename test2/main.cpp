﻿#include "main.h"

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

[[nodiscard]] inline int MakeSocketFD(int port, int sockType = SOCK_STREAM, char const* hostName = nullptr) {
    int fd{-1};
    addrinfo hints;
    bzero(&hints, sizeof(addrinfo));
    hints.ai_family = AF_UNSPEC;  // ipv4 / 6
    hints.ai_socktype = sockType; // SOCK_STREAM / SOCK_DGRAM
    hints.ai_flags = AI_PASSIVE;  // all interfaces
    addrinfo *ai_{}, *ai;
    if (getaddrinfo(hostName, std::to_string(port).c_str(), &hints, &ai_)) return -1; // format error?
    for (ai = ai_; ai != nullptr; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
        if (fd == -1) continue;
        int enable = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            close(fd);
            continue;
        }
        if (!bind(fd, ai->ai_addr, ai->ai_addrlen)) break; // success
        close(fd);
    }
    freeaddrinfo(ai_);
    if (!ai) return -2; // address not found?
    return fd;
}

struct Epoll;
struct Socket {

    Epoll* ep{};
    int fd{-1};
    uint32_t events{};
    xx::Coro OnEvents;  // need assign

    Socket() = delete;
    Socket(Socket const&) = delete;
    Socket& operator=(Socket const&) = delete;

    Socket(Epoll* ep_, int fd_);    // ep = ep_, fd = fd_
    ~Socket() {
        xx_assert(fd == -1);
    };

    void CloseFD();   // close(fd), fd=-1
};

struct Epoll {
    volatile int running{1};
    int efd{-1};
    addrinfo hints{};
    epoll_event event{};
    std::array<epoll_event, 4096> events{};
    std::array<xx::Shared<Socket>, 40000> sockets;
    std::array<uint8_t, 256 * 1024> buf{};
    std::array<iovec, UIO_MAXIOV> iovecs{};

    Epoll(Epoll const&) = delete;
    Epoll& operator=(Epoll const&) = delete;
    Epoll() {
        efd = epoll_create1(0);
        xx_assert(efd != -1);
    }
    ~Epoll() {
        if (efd != -1) {
            close(efd);
            efd = -1;
        }
    }

    template<typename T>
    inline void Add(T&& s) {
        sockets[s->fd] = std::forward<T>(s);
    }

    template<typename T>
    inline void Remove(T& s) {
        sockets[s->fd].Reset();
    }

    [[nodiscard]] inline int Ctl(int fd, uint32_t flags, int op = EPOLL_CTL_ADD) {
        bzero(&event, sizeof(event));
        event.data.fd = fd;
        event.events = flags;
        return epoll_ctl(efd, op, fd, &event);
    };

    inline void CtlDel(int fd) {
        epoll_ctl(efd, EPOLL_CTL_DEL, fd, nullptr);
    };

    [[nodiscard]] inline int Wait(int timeoutMS) {
        int n = epoll_wait(efd, events.data(), (int) events.size(), timeoutMS);
        if (n == -1) return errno;
        for (int i = 0; i < n; ++i) {
            auto fd = events[i].data.fd;
            if (fd >= sockets.size() || !sockets[fd]) {
                CtlDel(fd);
                close(fd);
                continue;
            }
            auto&& s = *sockets[fd];
            assert(s.fd == fd);

            s.events = events[i].events;
            // todo: if (s.OnEvents) remove s from ???  delay close ???
            s.OnEvents();
        }
        return 0;
    }
};

Socket::Socket(Epoll* ep_, int fd_) {
    xx_assert(ep == nullptr);
    xx_assert(fd == -1);
    xx_assert(ep_);
    xx_assert(fd_);
    xx_assert(fd_ < ep_->sockets.size());
    xx_assert(!ep_->sockets[fd_]);
    ep = ep_;
    fd = fd_;
}

inline void Socket::CloseFD() {
    if (fd == -1) return;
    xx_assert(ep);
    xx_assert(fd < (int)ep->sockets.size());
    xx_assert(ep->sockets[fd]);
    xx_assert(ep->sockets[fd].pointer == this);
    close(fd);
    fd = -1;
}

/*******************************************************************************************************/
/*******************************************************************************************************/

// member func exists checkers
template<typename T> concept TcpPeerBase_OnAccept = requires(T t) { t.OnAccept(); };
template<typename T> concept TcpPeerBase_OnReceive = requires(T t) { t.OnReceive(); };
template<typename T> concept TcpPeerBase_OnClose = requires(T t) { t.OnClose(); };


template<typename Peer>
struct [[maybe_unused]] TcpListener : Socket {
    using Socket::Socket;

    int Listen() {
        if (auto r = listen(fd, SOMAXCONN); r == -1) return -1;
        if (auto r = ep->Ctl(fd, EPOLLIN); r == -1) return -2;
        OnEvents = OnEvents_();
        ep->Add( xx::SharedFromThis(this) );
        return 0;
    }
    ~TcpListener() {
        CloseFD();
    }
    xx::Coro OnEvents_() {
        while(true) {
            CoYield;
            if(events & EPOLLERR || events & EPOLLHUP) break;
            while (HandleEvent() == 0);
        }
        CloseFD();
    }
    int HandleEvent() {
        sockaddr_in6 addr{};
        socklen_t len = sizeof(addr);
        int s = accept(fd, (sockaddr *) &addr, &len);
        if (s < 0) return -1;   // system error?
        xx_assert(s < (int) ep->sockets.size());
        xx_assert(!ep->sockets[s]);
        auto sg = xx::MakeScopeGuard([&] { close(s); });
        if (-1 == fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK)) return -2;  // system error?
        if (-1 == ep->Ctl(s, EPOLLIN)) return -3;   // system error?
        auto p = xx::Make<Peer>(ep, s, addr);
        if constexpr (TcpPeerBase_OnAccept<Peer>) {
            if (p->OnAccept()) return 0;
        }
        ep->sockets[s] = std::move(p);
        sg.Cancel();
        return 0;
    }
};

/*******************************************************************************************************/
/*******************************************************************************************************/

template<typename Derived>
struct TcpPeerBase : Socket {
    using Socket::Socket;

    sockaddr_in6 addr{};
    bool sending{};
    xx::DataQueue sendQueue;
    xx::Data recv;

    /* derived interface:
    int OnAccept() {   // return !0: do not add to ep->socket & kill
        ...
        return 0;
    }
    void OnReceive() {
        ...
        recv.Clear();
    }
    void OnClose() {
        ...
    }
    */

    TcpPeerBase(Epoll* ep_, int fd_, sockaddr_in6 const& addr_)
        : Socket(ep_, fd_)
        , addr(addr_) {
        OnEvents = OnEvents_();
    }

    [[maybe_unused]] inline int Send(xx::Data &&data) {
        sendQueue.Push(std::move(data));
        return !sending ? Send() : 0;
    }

    int Send() {
        if (!sendQueue.bytes) return ep->Ctl(fd, EPOLLIN, EPOLL_CTL_MOD);
        auto bufLen = ep->buf.size();   // max len
        int vsLen = 0;  // num blocks
        auto &&offset = sendQueue.Fill(ep->iovecs, vsLen, bufLen);
        ssize_t sentLen;
        do {
            sentLen = writev(fd, ep->iovecs.data(), vsLen);
        } while (sentLen == -1 && errno == EINTR);
        //memset(ep->iovecs.data(), 0, vsLen * sizeof(iovec)); // for valgrind
        if (sentLen == 0) return -1; // disconnected?
        else if (sentLen == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS) goto LabEnd; // can't send, need wait
            else return -2; // error?
        }
        else if ((size_t) sentLen == bufLen) {
            sendQueue.Pop(vsLen, offset, bufLen);
            return 0; // all sent
        }
        else {
            sendQueue.Pop(sentLen); // partial sent, need wait
        }
        LabEnd:
        sending = true;
        return ep->Ctl(fd, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD); // wait & watch writable state
    }

    xx::Coro OnEvents_() {
        while(true) {
            CoYield;
            if (events & EPOLLERR || events & EPOLLHUP) break;  // log?
            if (events & EPOLLIN) {
                if (!recv.cap) {
                    recv.Reserve(ep->buf.size());
                }
                if (recv.len == recv.cap) break;    // log?  // too many data not handled
                if (auto len = read(fd, recv.buf + recv.len, recv.cap - recv.len); len <= 0) break; // log? read failed
                else {
                    recv.len += len;
                }
                if constexpr (TcpPeerBase_OnReceive<Derived>) {
                    ((Derived *) this)->OnReceive();
                    if (fd == -1) break;
                } else {
                    Send(xx::Data(recv));   // default: echo logic
                    recv.Clear();
                }
            }
            if (events & EPOLLOUT) {
                sending = false;
                if (Send()) break;  // log?
            }
        }
        CloseFD();
        if constexpr (TcpPeerBase_OnClose<Derived>) {
            ((Derived *) this)->OnClose();
        }
    }
};

/*******************************************************************************************************/
/*******************************************************************************************************/

struct EchoPeer : TcpPeerBase<EchoPeer> {
    using Base = TcpPeerBase<EchoPeer>;
    using Base::Base;
    int OnAccept() {
        xx::CoutN("peer accepted. ip = ", addr);
        return 0;
    }
    void OnReceive() {
        Send(xx::Data(recv));
        recv.Clear();
    }
    void OnClose() {
        xx::CoutN("peer closed. ip = ", addr);
    }
};

int main() {
    xx::CoutN("begin");
    Epoll ep;

    xx::CoutN("init listener");
    if (int r = xx::Make<TcpListener<EchoPeer>>( &ep, MakeSocketFD(12345) )->Listen()) return r;

    xx::CoutN("running...");
    while(ep.running) {
        if (int r = ep.Wait(1)) return r;
    }
	return 0;
}