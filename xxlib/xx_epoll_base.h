#pragma once

#include "xx_coros.h"
#include "xx_ptr.h"
#include "xx_data_queue.h"
#include "xx_data_shared.h"
#include "xx_listdoublelink.h"

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

namespace xx::net {

    void CloseFD(int fd) {
        for (int r = close(fd); r == -1 && errno == EINTR;);
    }

    inline int FillAddress(std::string const &ip, int const &port, sockaddr_in6 &addr) {
        bzero(&addr, sizeof(addr));

        if (ip.find(':') == std::string::npos) {        // ipv4
            auto a = (sockaddr_in *) &addr;
            a->sin_family = AF_INET;
            a->sin_port = htons((uint16_t) port);
            if (!inet_pton(AF_INET, ip.c_str(), &a->sin_addr)) return -1;
        } else {                                            // ipv6
            auto a = &addr;
            a->sin6_family = AF_INET6;
            a->sin6_port = htons((uint16_t) port);
            if (!inet_pton(AF_INET6, ip.c_str(), &a->sin6_addr)) return -1;
        }

        return 0;
    }

    inline sockaddr_in6 ToAddress(std::string const &ip, int const &port) {
        sockaddr_in6 rtv;
        xx_assert(-1 != FillAddress(ip, port, rtv));
        return rtv;
    }


    // for create listener
    template<bool enableReuseAddr = true>
    [[nodiscard]] int MakeListenerSocketFD(int port, int sockType = SOCK_STREAM, char const *hostName = {}) {
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
            if constexpr (enableReuseAddr) {
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
    [[nodiscard]] int MakeAcceptSocketFD(int listenerFD, sockaddr_in6 *addr) {
        socklen_t len = sizeof(sockaddr_in6);
        if (int fd = accept4(listenerFD, (sockaddr *) addr, &len, SOCK_NONBLOCK); fd == -1) {
            if (auto e = errno; e == EAGAIN) return 0;  // other process handled?
            else return -e;
        } else return fd;
    }

    // for client
    [[nodiscard]] int MakeSocketFD() {
        auto fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        return fd == -1 ? -errno : fd;
    }

    // read all data from fd. append to d
    // success: return 0
    // < 0 == -errno, need close fd
    template<size_t reserveLen = 1024 * 256, size_t maxLen = 0/*1024 * 1024 * 4*/>
    [[nodiscard]] int ReadData(int fd, xx::Data &d) {
        LabBegin:
        d.Reserve(reserveLen);
        auto span = d.GetFreeRange();
        assert(span.len);
        LabRepeat:
        if (auto n = ::read(fd, span.buf, span.len); n > 0) {
            d.len += n;
            if constexpr (maxLen) {
                if (d.len > maxLen) return -9999;
            }
            if (n == span.len) goto LabBegin;
            return 0;
        } else if (n == 0) return -999;
        else if (auto e = errno; e == EAGAIN || e == EWOULDBLOCK) goto LabRepeat;
        else return -e;
    }

    // write data to fd
    // success: return "the number of bytes written" + blocked
    // < 0 == -errno, need close fd
    [[nodiscard]] std::pair<ssize_t, bool> WriteData(int fd, void *buf_, size_t len_) {
        auto buf = (char *) buf_;
        auto len = len_;
        LabRepeat:
        if (auto n = ::write(fd, buf, len); n != -1) {
            assert(n <= len);
            if (len -= n; len > 0) {
                buf += n;
                goto LabRepeat;
            } else return {(ssize_t) len_, false};  // success
        } else {
            if (auto e = errno; e == EAGAIN || e == EWOULDBLOCK) return {(char *) buf_ - buf + n, true}; // success
            else return {-e, false};   // error
        }
    }

    // when sizeof(LenType) == sizeofLen, same as xx::Data::ReadFixed
    template<typename LenType = uint32_t, size_t sizeofLen = 4>
    LenType ReadPackageLen(uint8_t const *buf) {
        static_assert(sizeofLen >= 1 && sizeofLen <= sizeof(LenType));
        LenType rtv{};
        memcpy(&rtv, buf, sizeofLen);
        if constexpr (std::endian::native == std::endian::big) {
            rtv = ::xx::BSwap(rtv);
        }
        return rtv;
    }

    // when sizeof(LenType) == sizeofLen, same as xx::Data::WriteFixed
    template<typename LenType = uint32_t, size_t sizeofLen = 4>
    void WritePackageLen(uint8_t *buf, LenType v) {
        static_assert(sizeofLen >= 1 && sizeofLen <= sizeof(LenType));
        if constexpr (std::endian::native == std::endian::big) {
            v = ::xx::BSwap(v);
        }
        memcpy(buf, &v, sizeofLen);
    }

    struct FdBase {
        int fd{-1};
        mutable int errorNumber{};

        int (*OnEvents__)(FdBase *s, uint32_t e){};    // epoll events handler. return non 0: CloseFD + CtlDel

        FdBase() = default;

        FdBase(FdBase const &) = delete;

        FdBase &operator=(FdBase const &) = delete;

        ~FdBase() {
            xx_assert(fd == -1);
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

        int Create() {
            return (fd = epoll_create(1));
        }

        // op == EPOLL_CTL_ADD, EPOLL_CTL_MOD
        // events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLET
        template<int op = EPOLL_CTL_ADD, uint32_t events = EPOLLIN | EPOLLOUT | EPOLLET>
        [[nodiscard]] int Ctl(int socketFD, void *ptr) const {
            epoll_event ee{events, ptr};
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

    using IdxVerType = xx::ListDoubleLinkIndexAndVersion<int, uint>;    // int + uint  for store to  epoll int64 user data

    template<typename NetCtxType>
    struct Socket : FdBase {
        using FdBase::FdBase;

        NetCtxType *nc{};
        IdxVerType iv;
        sockaddr_in6 addr{};

        void AddCondTaskToNC(xx::Task<> &&c) {
            nc->condTasks.Add(xx::WeakFromThis(this), std::move(c));
        }
    };

    template<typename NetCtxType>
    struct Connector : Socket<NetCtxType> {
        bool connected{}, hasEpollIn{};

        int OnEvents(uint32_t e) {
            if (e & EPOLLERR || e & EPOLLHUP) return -888;    // fatal error
            int err{};
            socklen_t result_len = sizeof(err);
            if (-1 == getsockopt(this->fd, SOL_SOCKET, SO_ERROR, &err, &result_len)) return -errno;
            if (err) return -err;
            connected = true;
            if (e & EPOLLIN) {
                hasEpollIn = true;
            }
            return 0;
        }
    };

    template<typename NetCtxType, typename PeerType>
    struct ListenerBase : Socket<NetCtxType> {
        using Socket<NetCtxType>::Socket;

        ~ListenerBase() {
            this->FdDispose();
        }

        int OnEvents(uint32_t e);
    };

    template<typename T> concept Has_OnAccept = requires(T t) { t.OnAccept(); };
    template<typename T> concept Has_OnUpdate = requires(T t) { t.OnUpdate(); };
    template<typename T> concept Has_OnRunOnce = requires(T t) { t.OnRunOnce(); };

    template<typename Derived>
    struct NetCtxBase : Epoll {
        using SocketType = Socket<NetCtxBase>;
        using Epoll::Epoll;

        std::array<epoll_event, 4096> events{}; // tmp epoll events container
        xx::ListDoubleLink<xx::Shared<Socket<Derived>>, int, uint> sockets;    // listeners + accepted peers container
        IdxVerType lastListenerIV;  // for visit all sockets, skip listeners ( Foreach( []{}, Next( iv ) )

        xx::Tasks tasks;
        xx::CondTasks<xx::Weak<Socket<Derived>>> condTasks;

        void AddTask(xx::Task<> &&c) {
            tasks.Add(std::move(c));
        }

        NetCtxBase() {
            xx_assert(-1 != Create());
        }

        ~NetCtxBase() {
            FdDispose();
        }

        void SocketFill(Socket<Derived> &s, int fd_, IdxVerType iv_, sockaddr_in6 const *addr_ = {}) {
            xx_assert(s.fd == -1);
            xx_assert(!s.nc);
            xx_assert(s.iv.index == -1 && s.iv.version == 0);
            s.fd = fd_;
            s.nc = (Derived *) this;
            s.iv = iv_;
            if (addr_) {
                s.addr = *addr_;
            } else {
                bzero(&s.addr, sizeof(s.addr));
            }
        }

        void SocketDispose(Socket<Derived> &s) {
            xx_assert(-1 != CtlDel(s.fd));
            s.FdDispose();
            xx_assert(sockets.Exists(s.iv));
            xx_assert(sockets[s.iv].pointer == &s);
            sockets.Remove(s.iv);
        }

        // sockets[s.iv] = move(t)
        template<typename PeerType>
        void SocketReplace(Socket<Derived> &s, xx::Shared<PeerType> &&t) {
            xx_assert(s.fd != -1);
            xx_assert(s.nc == this);
            xx_assert(sockets[s.iv].pointer == &s);
            xx_assert(t);
            xx_assert(t->fd == -1);
            xx_assert(t->iv.index == -1);
            t->fd = s.fd;
            t->nc = s.nc;
            t->iv = s.iv;
            t->addr = s.addr;
            s.fd = -1;
            s.nc = {};
            s.iv = {};
            sockets[t->iv] = std::move(t);
        }

        // create listener & listen & return weak
        template<typename PeerType, class = std::enable_if_t<std::is_base_of_v<Socket<Derived>, PeerType>>>
        int Listen(int port, int sockType = SOCK_STREAM, char const *hostName = {}) {
            int r = MakeListenerSocketFD(port, sockType, hostName);
            xx_assert(r >= 0);
            xx_assert(-1 != listen(r, SOMAXCONN));
            auto &c = sockets.Emplace();    // shared ptr container
            auto iv = sockets.Tail();
            auto rtv = Ctl<EPOLL_CTL_ADD, (uint32_t) EPOLLIN>(r, (void *&) iv);
            xx_assert(!rtv);
            auto L = xx::Make<ListenerBase<Derived, PeerType>>();
            SocketFill(*L, r, iv);
            L->OnEvents__ = [](FdBase *s, uint32_t e) { return ((ListenerBase<Derived, PeerType> *) s)->OnEvents(e); };
            c = L;
            lastListenerIV = iv;
            return r;
        }

        template<typename PeerType>
        xx::Weak<PeerType> MakePeer(int fd_, sockaddr_in6 const &addr) {
            auto &c = sockets.Emplace();    // shared ptr container
            auto iv = sockets.Tail();
            xx_assert(Ctl(fd_, (void *&) iv) >= 0);
            auto &s = c.template Emplace<PeerType>();
            SocketFill(*s, fd_, iv, &addr);
            s->OnEvents__ = [](FdBase *s, uint32_t e) { return ((PeerType *) s)->OnEvents(e); };
            if constexpr (Has_OnAccept<PeerType>) {
                if (s->OnAccept()) {
                    SocketDispose(*s);
                    return {};
                }
            }
            return s.ToWeak();
        }

        // return sockets.size
        int Wait(int timeoutMS) {
            xx_assert(fd != -1);    // forget Init ?
            int n = epoll_wait(fd, events.data(), (int) events.size(), timeoutMS);
            xx_assert(n != -1);
            for (int i = 0; i < n; ++i) {
                auto &iv = (IdxVerType &) events[i].data.ptr;
                xx_assert(sockets.Exists(iv));
                auto &s = sockets[iv];
                xx_assert(s);
                if (int r = s->OnEvents__(s.pointer, events[i].events)) {
                    SocketDispose(*s);  // log?
                }
            }
            return sockets.Count();
        }

        int RunOnce(int timeoutMS) {
            int r = Wait(timeoutMS);
            r += tasks();
            r += condTasks();
            if constexpr (Has_OnRunOnce<Derived>) {
                r += ((Derived*)this)->OnRunOnce();
            }
            return r;
        }

        template<typename PeerType, typename R = std::pair<int, xx::Weak<PeerType>>>
        xx::Task<R> Connect(sockaddr_in6 addr, double timeoutSecs) {
            auto fd = MakeSocketFD();
            if (fd < 0) co_return R{ fd, {} };
            if (auto r = connect(fd, (sockaddr *) &addr, sizeof(addr)); r == 0) {
                co_return R{ 0, MakePeer<PeerType>(fd, addr) };
            } else if (auto e = errno; e == EINPROGRESS) {        // r == -1
                auto secs = xx::NowSteadyEpochSeconds();
                auto w = MakePeer<Connector<Derived>>(fd, addr);
                while (true) {
                    co_yield 0;
                    if (!w) co_return R{ -88888, {} };   // fd error
                    if (w->connected) { // success: replace
                        R rtv;
                        auto hasEpollIn = w->hasEpollIn;
                        auto s = xx::Make<PeerType>();
                        rtv.second = s;
                        s->OnEvents__ = [](FdBase *s, uint32_t e) { return ((PeerType *) s)->OnEvents(e); };
                        SocketReplace(*w, std::move(s));
                        if (hasEpollIn) {
                            if (int rr = rtv.second->OnEvents(EPOLLIN)) {
                                SocketDispose(*rtv.second);  // log?
                                rtv.first = rr;
                                rtv.second.Reset();
                            }
                        }
                        co_return rtv;
                    }
                    if (xx::NowSteadyEpochSeconds() - secs > timeoutSecs) { // timeout:
                        SocketDispose(*w);
                        co_return R{ -99999, {} };
                    }
                }
            } else co_return R{ -errno, {} };
        }
    };


    template<typename NetCtxType, typename PeerType>
    int ListenerBase<NetCtxType, PeerType>::OnEvents(uint32_t e) {
        if (e & EPOLLERR || e & EPOLLHUP) return -888;    // fatal error
        auto HandleEvent = [this] {
            sockaddr_in6 addr{};
            int s = MakeAcceptSocketFD(this->fd, &addr);
            if (s == 0) return 1;   // no more accept?
            else if (s < 0) return -errno;   // system error?
            this->nc->template MakePeer<PeerType>(s, addr);
            return 0;
        };
        LabRepeat:
        int r = HandleEvent();
        if (r == 0) goto LabRepeat;
        else if (r == 1) return 0;
        else return r;
    }


    template<typename NetCtxType>
    struct TcpSocket : Socket<NetCtxType> {
        using Socket<NetCtxType>::Socket;

        xx::Queue<xx::DataShared> sents;
        size_t sentsTopOffset{};

        // call by events & EPOLLOUT
        int Send() {
            while (!sents.Empty()) {
                auto &d = sents.Top();
                auto [n, b] = ::xx::net::WriteData(this->fd, d.GetBuf() + sentsTopOffset, d.GetLen() - sentsTopOffset);
                if (n < 0) return (int) n;
                if (b) {
                    sentsTopOffset -= n;
                    break;
                } else {
                    sentsTopOffset = 0;
                    sents.Pop();
                }
            }
            return 0;
        }

        int Send(void *buf, size_t len) {
            if (sents.Empty()) {
                auto [n, b] = ::xx::net::WriteData(this->fd, buf, len);
                if (n < 0) return (int) n;
                assert(b && n < len || !b && n == len);
                if (b) {
                    sents.Emplace(xx::Data((char *) buf + n, len - n));
                }
            } else {
                sents.Emplace(xx::Data(buf, len));
            }
            return 0;
        }

        template<typename DataType>
        int Send(DataType &&d) {
            auto buf = d.GetBuf();
            auto len = d.GetLen();
            if (sents.Empty()) {
                auto [n, b] = ::xx::net::WriteData(this->fd, buf, len);
                if (n < 0) return (int) n;
                assert(b && n < len || !b && n == len);
                if (b) {
                    sents.Emplace(xx::Data((char *) buf + n, len - n));
                }
            } else {
                sents.Emplace(std::forward<DataType>(d));
            }
            return 0;
        }
    };

    /*******************************************************************************************************/
    /*******************************************************************************************************/

    //template<typename T> concept Has_OnEventsIn = requires(T t) { t.OnEventsIn(); };

    // base code : OnEvents
    template<typename Derived, size_t reserveLen = 1024 * 256, size_t maxLen = 0>
    struct PartialCodes_OnEvents_Base {

        xx::Data recv;  // received data container

        int OnEvents(uint32_t e) {
            if (e & EPOLLERR || e & EPOLLHUP) return -888;    // fatal error
            if (e & EPOLLOUT) {
                if (int r = ((Derived*)this)->Send()) return r;
            }
            if (e & EPOLLIN) {
                if (int r = xx::net::ReadData<reserveLen, maxLen>(((Derived*)this)->fd, recv)) return r;
                if (int r = ((Derived*)this)->OnEventsIn()) return r;
            }
            return 0;
        }
    };

    // for combine & split package data
    // Derived interface: int OnEventsPkg(xx::Data_r dr)
    template<typename Derived, typename LenType = uint32_t, size_t sizeofLen = 4, bool lenContainSelf = false, size_t reserveLen = 1024 * 256, size_t maxLen = 0>
    struct PartialCodes_OnEvents_Pkg : PartialCodes_OnEvents_Base<Derived, reserveLen, maxLen> {

        [[nodiscard]] constexpr size_t GetReserveLen() const { return reserveLen; }

        int OnEventsIn() {
            auto& recv = ((Derived*)this)->recv;
            size_t offset = 0;
            while (offset + sizeofLen <= recv.len) {
                auto len = ReadPackageLen<LenType, sizeofLen>(recv.buf + offset);
                if constexpr (maxLen) {
                    if (len > maxLen) return -777;
                }
                if constexpr (!lenContainSelf) {
                    len += sizeofLen;
                }
                if (offset + len > recv.len) break; // incomplete
                if (int r = ((Derived *) this)->OnEventsPkg({recv.buf + offset, len})) return r;
                offset += len;
            }
            recv.RemoveFront(offset);
            return 0;
        }
    };
}
