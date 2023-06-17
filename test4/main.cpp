#include "main.h"

void CloseFD(int fd) {
    for (int r = close(fd); r == -1 && errno == EINTR;);
}

// for create listener or client socket fd
template<bool enableReuseAddr = true>
[[nodiscard]] int MakeSocketFD(int port, int sockType = SOCK_STREAM, char const* hostName = nullptr) {
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
    FdBase() = default;
    FdBase(FdBase const&) = delete;
    FdBase& operator=(FdBase const&) = delete;
    ~FdBase() {
        xx_assert(fd != -1);    //DisposeFd();
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
        return epoll_ctl(fd, op, socketFD, &ee);
    }

    // op == EPOLL_CTL_DEL
    [[nodiscard]] int CtlDel(int socketFD) const {
        return epoll_ctl(fd, EPOLL_CTL_DEL, socketFD, nullptr);
    }
};

struct Socket : FdBase {
    using FdBase::FdBase;
    Epoll* ep{};
    xx::ListDoubleLinkIndexAndVersion<> iv;
    sockaddr_in6 addr{};
    void(*OnEvents)(Socket* s, uint32_t e){} ;  // example:  = [](Socket* s, uint32_t e) { ((CLASSNAME*)s)->OnEvents_(e); };
};

/*******************************************************************************************************/
/*******************************************************************************************************/

struct NetCtxBase;

template<typename PeerType>
struct ListenerBase : Socket {
    using Socket::Socket;
    void Init() {
        OnEvents = [](Socket* s, uint32_t e) { ((ListenerBase<PeerType>*)s)->OnEvents_(e); };
    }
    void OnEvents_(uint32_t e);
};

template<typename T> concept Has_OnAccept = requires(T t) { t.OnAccept(); };
template<typename T> concept Has_OnReceive = requires(T t) { t.OnReceive(); };
template<typename T> concept Has_OnClose = requires(T t) { t.OnClose(); };

struct NetCtxBase : Epoll {
    using Epoll::Epoll;
    xx::ListDoubleLink<xx::Shared<Socket>> listeners, sockets;

    template<typename ListenerType>
    int Listen(int port, int sockType = SOCK_STREAM, char const* hostName = nullptr) {
        int r = MakeSocketFD(port, sockType, hostName);
        if (r < 0) return r;
        auto& L = listeners.Emplace().Emplace<ListenerType>();
        L->fd = r;
        L->ep = this;
        L->iv = listeners.Tail();
        L->Init();
        return 0;
    }

    template<typename PeerType>
    int MakePeer(int fd_, sockaddr_in6 const& addr) {
        auto& s = sockets.Emplace().Emplace<PeerType>();
        s->fd = fd_;
        s->ep = this;
        s->iv = sockets.Tail();
        s->addr = addr;
        auto ws = s.ToWeak();
        if (-1 == Ctl(fd_, ws.h)) return -1;   // system error?
        ws.h = nullptr; // hack
        if constexpr (Has_OnAccept<PeerType>) {
            s->OnAccept();
        }
        return 0;
    }
};

template<typename PeerType>
void ListenerBase<PeerType>::OnEvents_(uint32_t e) {
    assert(!(e & EPOLLERR || e & EPOLLHUP));
    auto HandleEvent = [this] {
        sockaddr_in6 addr{};
        int s = MakeAcceptFD(fd, &addr);
        if (s < 0) return -1;   // no more accept or system error?
        ((NetCtxBase*)ep)->template MakePeer<PeerType>(s, addr);
        return 0;
    };
    while (HandleEvent() == 0);
}


/*******************************************************************************************************/
/*******************************************************************************************************/

struct Peer : Socket {
};

int main() {
    NetCtxBase nc;
    nc.Listen<ListenerBase<Peer>>(12345);
    //
    return 0;
}
