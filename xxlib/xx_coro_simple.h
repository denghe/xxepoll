#pragma once

// important: only support static function or lambda !!!  COPY data from arguments !!! do not ref !!!

#include "xx_typetraits.h"
#include "xx_time.h"
#include "xx_list.h"
#include "xx_listlink.h"

namespace xx {
    template<typename R> struct CoroBase_promise_type { R r; };
    template<> struct CoroBase_promise_type<void> {};
    template<typename R>
    struct Coro_ {
        struct promise_type : CoroBase_promise_type<R> {
            Coro_ get_return_object() { return { H::from_promise(*this) }; }
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept(true) { return {}; }
            template<typename U>
            std::suspend_always yield_value(U&& v) { if constexpr(!std::is_void_v<R>) this->r = std::forward<U>(v); return {}; }
            void return_void() {}
            void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
        };
        using H = std::coroutine_handle<promise_type>;
        H h;
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
    };
    using Coro = Coro_<void>;


#define CoYield co_yield std::monostate()
#define CoReturn co_return
#define CoAwait( coType ) { auto&& c = coType; while(!c) { CoYield; c(); } }
#define CoSleep( duration ) { auto tp = std::chrono::steady_clock::now() + duration; do { CoYield; } while (std::chrono::steady_clock::now() < tp); }


    template<>
    struct IsPod<Coro> : std::true_type {};
    template<typename T>
    struct IsPod<Coro_<T>> : IsPod<T> {};

    /***********************************************************************************************************/
    /***********************************************************************************************************/

    template<typename T> struct CorosBase {
        ListLink<std::pair<T, Coro>, int32_t> coros;
    };
    template<> struct CorosBase<void> {
        ListLink<Coro, int32_t> coros;
    };

    template<typename T>
    struct Coros_ : CorosBase<T> {
        Coros_(Coros_ const&) = delete;
        Coros_& operator=(Coros_ const&) = delete;
        Coros_(Coros_&&) noexcept = default;
        Coros_& operator=(Coros_&&) noexcept = default;
        explicit Coros_(int32_t cap = 8) {
            this->coros.Reserve(cap);
        }

        template<typename U>
        void Add(U&& t, Coro&& c) {
            if (c) return;
            this->coros.Emplace(std::forward<U>(t), std::move(c));
        }

        void Add(Coro&& c) {
            if (c) return;
            this->coros.Emplace(std::move(c));
        }

        void Clear() {
            this->coros.Clear();
        }

        int32_t operator()() {
            int prev = -1, next{};
            for (auto idx = this->coros.head; idx != -1;) {
                auto& o = this->coros[idx];
                bool needRemove;
                if constexpr(std::is_void_v<T>) {
                    needRemove = o.Resume();
                } else {
                    needRemove = !o.first || o.second.Resume();
                }
                if (needRemove) {
                    next = this->coros.Remove(idx, prev);
                } else {
                    next = this->coros.Next(idx);
                    prev = idx;
                }
                idx = next;
            }
            return this->coros.Count();
        }

        [[nodiscard]] int32_t Count() const {
            return this->coros.Count();
        }

        [[nodiscard]] bool Empty() const {
            return !this->coros.Count();
        }

        void Reserve(int32_t cap) {
            this->coros.Reserve(cap);
        }
    };

    using Coros = Coros_<void>;

    // check condition before Resume ( for life cycle manage )
    template<typename T>
    using CondCoros = Coros_<T>;


    /***********************************************************************************************************/
    /***********************************************************************************************************/

    template<typename KeyType, typename DataType>
    using EventArgs = std::pair<KeyType, DataType&>;

    template<typename KeyType, typename DataType>
    using EventCoro = Coro_<std::variant<std::monostate, EventArgs<KeyType, DataType>, int>>;   // int for stop coroutine run

    template<typename KeyType, typename DataType, int timeoutSecs = 15>
    struct EventCoros {
        using CoroType = EventCoro<KeyType, DataType>;
        using Args = EventArgs<KeyType, DataType>;
        using Tup = std::tuple<Args, double, CoroType>;
        xx::List<Tup, int> condCoros;
        xx::List<CoroType, int> updateCoros;

        int Add(CoroType&& c) {
            if (c) return 0;
            auto& r = c.h.promise().r;
            if (r.index() == 0) {
                updateCoros.Emplace(std::move(c));
            } else if (r.index() == 1) {
                condCoros.Emplace(std::move(std::get<Args>(r)), xx::NowSteadyEpochSeconds() + timeoutSecs, std::move(c) );
            } else if (r.index() == 2) return std::get<int>(r);
            return 0;
        }

        // match key & store d & resume coro
        // null: dismatch     0: success      !0: error ( need close ? )
        template<typename DT = DataType>
        std::optional<int> Trigger(KeyType const& v, DT&& d) {
            if (condCoros.Empty()) return false;
            for (int i = condCoros.len - 1; i >= 0; --i) {
                auto& tup = condCoros[i];
                if (v == std::get<Args>(tup).first) {
                    std::get<Args>(tup).second = std::forward<DT>(d);
                    if (int r = Resume(i)) return r;
                    return 0;
                }
            }
            return {};
        }

        // handle condCoros timeout & resume updateCoros
        int Update() {
            if (!condCoros.Empty()) {
                auto now = xx::NowSteadyEpochSeconds();
                for (int i = condCoros.len - 1; i >= 0; --i) {
                    auto& tup = condCoros[i];
                    if (std::get<double>(tup) < now) {
                        std::get<Args>(tup).second = {};
                        if (int r = Resume(i)) return r;
                    }
                }
            }
            if (!updateCoros.Empty()) {
                for (int i = updateCoros.len - 1; i >= 0; --i) {
                    if (updateCoros[i].Resume()) {
                        updateCoros.SwapRemoveAt(i);
                    }
                }
            }
            return 0;
        }

    protected:
        XX_FORCE_INLINE int Resume(int i) {
            auto& tup = condCoros[i];
            auto& args = std::get<Args>(tup);
            auto& c = std::get<CoroType>(tup);
            if (c.Resume()) {
                condCoros.SwapRemoveAt(i);
            } else {
                auto& r = c.h.promise().r;
                if (r.index() == 0) {
                    updateCoros.Emplace(std::move(c));
                    condCoros.SwapRemoveAt(i);
                } else if (r.index() == 1) {
                    args = std::move(std::get<Args>(r));
                    std::get<double>(tup) = xx::NowSteadyEpochSeconds() + timeoutSecs;
                } else return std::get<int>(r);
            }
            return 0;
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
    xx::ECoros cs;

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