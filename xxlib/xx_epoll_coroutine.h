#pragma once
#include "xx_coro_simple.h"
#include "xx_ptr.h"
#include "xx_data_queue.h"

#include <csignal>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <sys/signalfd.h>
#include <sys/inotify.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

struct Epoll;
struct Socket {
    Epoll* ep{};
    int fd{-1};
    /* derived interface:
    int Init(Epoll* ep_, int fd_) {
        AssertEpFd(ep_, fd_);
        ep = ep_;
        fd = fd_;
        ...
        return 0;
    }
    */
    void AssertEpFd(Epoll* ep_, int fd_) const;
    [[nodiscard]] inline bool Alive() const { return fd != -1; }
    virtual void Close();   // unsafe
    virtual void HandleEvent(uint32_t e) = 0;
    virtual ~Socket() {
        Socket::Close();
    };
};

template<typename Peer>
struct [[maybe_unused]] TcpListenerBase : Socket {
    int Init(Epoll* ep_, int fd_);
    void HandleEvent(uint32_t e) override;
};

// member func exists checkers
template<typename T> concept TcpPeerBase_Accept = requires(T t) { t.Accept(); };

template<typename Derived>
struct TcpPeerBase : Socket {
    sockaddr_in6 addr{};
    bool sending = false;
    xx::DataQueue sendQueue;
    xx::Data recv;
    void Init(Epoll* ep_, int fd_, sockaddr_in6 const& addr_) {
        AssertEpFd(ep_, fd_);
        ep = ep_;
        fd = fd_;
        addr = addr_;
    }
    /* derived interface:
    int Accept() {   // return 0: success
        ...
        return 0;
    }
    int Receive() {   // return 0: success or can safety visit member
        ...
        recv.Clear();
        return 0;
    }
    */
    [[maybe_unused]] inline int Send(xx::Data &&data) {
        sendQueue.Push(std::move(data));
        return !sending ? Send() : 0;
    }
    int Send();
    void HandleEvent(uint32_t e) override;
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


    [[nodiscard]] inline int MakeSocket(int port, int sockType = SOCK_STREAM, char const* hostName = nullptr) {
        int fd{-1};

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

        xx_assert(fd < sockets.size());
        xx_assert(!sockets[fd]);
        return fd;
    }

    [[nodiscard]] inline int Ctl(int fd, uint32_t flags, int op = EPOLL_CTL_ADD) {
        bzero(&event, sizeof(event));
        event.data.fd = fd;
        event.events = flags;
        return epoll_ctl(efd, op, fd, &event);
    };

    [[nodiscard]] inline int CtlClose(int fd) const {
        if (fd == -1) return -1;
        epoll_ctl(efd, EPOLL_CTL_DEL, fd, nullptr);
        return close(fd);
    }

    [[nodiscard]] inline int Wait(int timeoutMS) {
        int n = epoll_wait(efd, events.data(), (int) events.size(), timeoutMS);
        if (n == -1) return errno;
        for (int i = 0; i < n; ++i) {
            auto fd = events[i].data.fd;
            auto&& h = sockets[fd];
            xx_assert(h->fd == fd);
            if (!h) {
                // log?
                (void)CtlClose(fd);
                continue;
            }
            auto e = events[i].events;
            h->HandleEvent(e);
        }
        return 0;
    }
};

void Socket::AssertEpFd(Epoll* ep_, int fd_) const {
    xx_assert(ep == nullptr);
    xx_assert(fd == -1);
    xx_assert(ep_);
    xx_assert(fd_);
    xx_assert(!ep_->sockets[fd_]);
}

inline void Socket::Close() {
    if (fd == -1) return;
    xx_assert(fd < (int)ep->sockets.size());
    xx_assert(ep->sockets[fd]);
    xx_assert(ep->sockets[fd].pointer == this);
    auto s = fd;
    fd = -1;
    (void)ep->CtlClose(s);
    ep->sockets[s].Reset(); // unsafe
}

template<typename Peer>
int TcpListenerBase<Peer>::Init(Epoll* ep_, int fd_) {
    AssertEpFd(ep_, fd_);
    ep = ep_;
    fd = fd_;
    if (-1 == listen(fd, SOMAXCONN)) return -1;
    if (-1 == ep->Ctl(fd, EPOLLIN)) return -2;
    ep->sockets[fd] = xx::SharedFromThis(this);
    return 0;
}

template<typename Peer>
void TcpListenerBase<Peer>::HandleEvent(uint32_t e) {
    xx_assert(!(e & EPOLLERR || e & EPOLLHUP));
    auto f = [&]{
        sockaddr_in6 addr{};
        socklen_t len = sizeof(addr);
        int s = accept(fd, (sockaddr *) &addr, &len);
        if (s < 0) return -1;
        xx_assert(s < (int) ep->sockets.size());
        xx_assert(!ep->sockets[s]);
        auto sg = xx::MakeScopeGuard([&] { close(s); });
        if (-1 == fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK)) return -2;
        if (-1 == ep->Ctl(s, EPOLLIN)) return -3;
        auto p = xx::Make<Peer>();
        p->Init(ep, s, addr);
        if constexpr( TcpPeerBase_Accept<Peer> ) {
            if (p->Accept()) return -4;
        }
        ep->sockets[s] = std::move(p);
        sg.Cancel();
        return 0;
    };
    while (f() == 0);
}

template<typename Derived>
inline int TcpPeerBase<Derived>::Send() {
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

template<typename Derived>
inline void TcpPeerBase<Derived>::HandleEvent(uint32_t e) {
    xx_assert(!(e & EPOLLERR || e & EPOLLHUP));
    if (e & EPOLLIN) {
        if (!recv.cap) {
            recv.Reserve(ep->buf.size());
        }
        if (recv.len == recv.cap) {
            Close();    // log?  // too many data not handled
            return;
        }
        if (auto len = read(fd, recv.buf + recv.len, recv.cap - recv.len); len <= 0) {
            Close();    // log? read failed
            return;
        } else {
            recv.len += len;
        }
        if (((Derived*)this)->Receive()) return;
        if (!Alive()) return;
    }
    if (e & EPOLLOUT) {
        sending = false;
        if (Send()) {
            // log?
            Close();
            return;
        }
    }
}
