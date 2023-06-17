#include "main.h"

void CloseFD(int fd) {
    for (int r = close(fd); r == -1 && errno == EINTR;);
}

// for listener
[[nodiscard]] int MakeAcceptFD(int listenerFD, sockaddr_in6* addr) {
    if (int fd = accept4(listenerFD, nullptr, nullptr, SOCK_NONBLOCK); fd == -1) {
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

struct Epoll {
    int efd{ -1 };
    mutable int errorNumber{};
    operator int() const {
        return efd;
    }

    Epoll() = default;
    Epoll(Epoll const&) = delete;
    Epoll& operator=(Epoll const&) = delete;
    Epoll(Epoll && o)  noexcept : efd(o.efd) { o.efd = -1; }
    Epoll& operator=(Epoll && o) { std::swap(efd, o.efd); return *this; }
    ~Epoll() {
        Dispose();
    }

    void Init() {
        efd = epoll_create(1);
    }

    void Dispose() {
        if (efd != -1) {
            CloseFD(efd);
            efd = -1;
        }
    }

    // op == EPOLL_CTL_ADD, EPOLL_CTL_MOD
    // events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLET
    template<int op = EPOLL_CTL_ADD, uint32_t events = EPOLLIN | EPOLLOUT | EPOLLET>
    [[nodiscard]] int Ctl(int socketFD, void* ptr) const {
        epoll_event ee {events, ptr};
        return epoll_ctl(efd, op, socketFD, &ee);
    }

    [[nodiscard]] int CtlDel(int socketFD) const {
        return epoll_ctl(efd, EPOLL_CTL_DEL, socketFD, nullptr);
    }

//    [[nodiscard]] int Shutdown(int socketFD) const {
//        if (Ctl<EPOLL_CTL_MOD, EPOLLHUP>(socketFD, (void*)(ssize_t)socketFD) == -1) {
//            errorNumber = errno;
//            return -1;
//        }
//        if (shutdown(socketFD, SHUT_WR) == -1) {
//            errorNumber = errno;
//            return -2;
//        }
//        return 0;
//    }
};




int main() {

}
