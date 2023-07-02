#pragma once

// important: only support static function or lambda !!!  COPY data from arguments !!! do not ref !!!

#include "xx_listlink.h"

namespace xx {
    template<typename R> struct CoroBase { R r; };
    template<> struct CoroBase<void> {};
    template<typename R>
    struct Coro_ {
        struct promise_type;
        using H = std::coroutine_handle<promise_type>;
        struct promise_type {
            Coro_ get_return_object() { return { H::from_promise(*this) }; }
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept(true) { return {}; }
            template<typename U>
            std::suspend_always yield_value(U&& v) { if constexpr(!std::is_void_v<R>) this->r = std::forward<U>(v); return {}; }
            void return_void() {}
            void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
        };

        Coro_() : h(nullptr) {}
        Coro_(H h) : h(h) {}
        ~Coro_() { if (h) h.destroy(); }
        Coro_(Coro_ const& o) = delete;
        Coro_& operator=(Coro_ const&) = delete;
        Coro_(Coro_&& o) noexcept : h(o.h) { o.h = nullptr; };
        Coro_& operator=(Coro_&& o) noexcept { std::swap(h, o.h); return *this; };

        void operator()() { h.resume(); }
        operator bool() const { return h.done(); }
        bool Resume() { h.resume(); return h.done(); }

    protected:
        H h;
    };
    using Coro = Coro_<void>;


#define CoYield co_yield 0
#define CoReturn co_return
#define CoAwait( coType ) { auto&& c = coType; while(!c) { CoYield; c(); } }
#define CoSleep( duration ) { auto tp = std::chrono::steady_clock::now() + duration; do { CoYield; } while (std::chrono::steady_clock::now() < tp); }


    template<>
    struct IsPod<Coro> : std::true_type {};

    struct Coros {
        Coros(Coros const&) = delete;
        Coros& operator=(Coros const&) = delete;
        Coros(Coros&&) noexcept = default;
        Coros& operator=(Coros&&) noexcept = default;
        explicit Coros(int32_t cap = 8) {
            coros.Reserve(cap);
        }

        ListLink<Coro, int32_t> coros;

        void Add(Coro&& g) {
            if (g) return;
            coros.Emplace(std::move(g));
        }

        void Clear() {
            coros.Clear();
        }

        int32_t operator()() {
            int prev = -1, next{};
            for (auto idx = coros.head; idx != -1;) {
                if (coros[idx].Resume()) {
                    next = coros.Remove(idx, prev);
                } else {
                    next = coros.Next(idx);
                    prev = idx;
                }
                idx = next;
            }
            return coros.Count();
        }

        [[nodiscard]] int32_t Count() const {
            return coros.Count();
        }

        [[nodiscard]] bool Empty() const {
            return !coros.Count();
        }

        void Reserve(int32_t cap) {
            coros.Reserve(cap);
        }
    };

    // check condition before Resume ( for life cycle manage )
    template<typename T>
    struct CorosByCond {
        CorosByCond(CorosByCond const&) = delete;
        CorosByCond& operator=(CorosByCond const&) = delete;
        CorosByCond(CorosByCond&&) noexcept = default;
        CorosByCond& operator=(CorosByCond&&) noexcept = default;
        explicit CorosByCond(int32_t cap = 8) {
            coros.Reserve(cap);
        }

        ListLink<std::pair<T, Coro>, int32_t> coros;

        template<typename U = T>
        void Add(U&& t, Coro&& g) {
            if (g) return;
            coros.Emplace(std::forward<U>(t), std::move(g));
        }

        void Clear() {
            coros.Clear();
        }

        int32_t operator()() {
            int prev = -1, next{};
            for (auto idx = coros.head; idx != -1;) {
                auto& kv = coros[idx];
                if (!kv.first || kv.second.Resume()) {
                    next = coros.Remove(idx, prev);
                } else {
                    next = coros.Next(idx);
                    prev = idx;
                }
                idx = next;
            }
            return coros.Count();
        }

        [[nodiscard]] int32_t Count() const {
            return coros.Count();
        }

        [[nodiscard]] bool Empty() const {
            return !coros.Count();
        }

        void Reserve(int32_t cap) {
            coros.Reserve(cap);
        }
    };
}

/*

example:

#include "xx_coro_simple.h"
#include <thread>
#include <iostream>

CoType func1() {
    CoSleep(1s);
}
CoType func2() {
    CoAwait( func1() );
    std::cout << "func 1 out" << std::endl;
    CoAwait( func1() );
    std::cout << "func 1 out" << std::endl;
}

int main() {
    xx::Coros cs;

    auto func = [](int b, int e)->CoType {
        CoYield;
        for (size_t i = b; i <= e; i++) {
            std::cout << i << std::endl;
            CoYield;
        }
        std::cout << b << "-" << e << " end" << std::endl;
    };

    cs.Add(func(1, 5));
    cs.Add(func(6, 10));
    cs.Add(func(11, 15));
    cs.Add([]()->CoType {
        CoSleep(1s);
        std::cout << "CoSleep 1s" << std::endl;
    }());
    cs.Add(func2());

LabLoop:
    cs();
    if (cs) {
        std::this_thread::sleep_for(500ms);
        goto LabLoop;
    }

    return 0;
}

*/