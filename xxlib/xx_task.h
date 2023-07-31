﻿#pragma once

#include "xx_typetraits.h"
#include "xx_time.h"
#include "xx_list.h"
#include "xx_listlink.h"
#include <xx_dict.h>
#include <xx_ptr.h>

namespace xx {
    template<typename R = void>
    struct Task;

    namespace detail {
        template<typename Derived, typename R>
        struct PromiseBase {
            std::coroutine_handle<> prev, last;
            PromiseBase *root{ this };
            std::variant<int, void*> y; // any ??? more type support ?

            struct FinalAwaiter {
                bool await_ready() const noexcept { return false; }
                void await_resume() noexcept {}
                template<typename promise_type>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> curr) noexcept {
                    if (auto &p = curr.promise(); p.prev) return p.prev;
                    else return std::noop_coroutine();
                }
            };
            Task<R> get_return_object() noexcept {
                auto tmp = Task<R>{ std::coroutine_handle<Derived>::from_promise(*(Derived*)this) };
                tmp.coro.promise().last = tmp.coro;
                return tmp;
            }
            template<typename P, class = std::enable_if_t<std::is_pointer_v<P>>>
            std::suspend_always yield_value(P const& p) {
                root->y = (void*)p;
                return {};
            }
            std::suspend_always yield_value(int v) {
                root->y = v;
                return {};
            }
            std::suspend_always initial_suspend() { return {}; }
            FinalAwaiter final_suspend() noexcept(true) { return {}; }
            void unhandled_exception() { throw; }
        };

        template<typename R>
        struct Promise final : PromiseBase<Promise<R>, R> {
            std::optional<R> r;

            template<typename T>
            void return_value(T&& v) { r.emplace(std::forward<T>(v)); }
        };

        template<>
        struct Promise<void> : PromiseBase<Promise<void>, void> {
            void return_void() noexcept {}
        };
    }

    /*************************************************************************************************************************/
    /*************************************************************************************************************************/

    template<typename R>
    struct [[nodiscard]] Task {
        using promise_type = detail::Promise<R>;
        using H = std::coroutine_handle<promise_type>;
        H coro;

        Task() = delete;
        Task(H h) { coro = h; }
        Task(Task const &o) = delete;
        Task &operator=(Task const &o) = delete;
        Task(Task &&o) : coro(std::exchange(o.coro, nullptr)) {}
        Task &operator=(Task &&o) {
            std::swap(coro, o.coro);
            return *this;
        }
        ~Task() { if (coro) { coro.destroy(); } }

        struct Awaiter {
            bool await_ready() const noexcept { return false; }
            decltype(auto) await_suspend(std::coroutine_handle<> prev) noexcept {
                auto& cp = curr.promise();
                cp.prev = prev;
                cp.root = ((H&)prev).promise().root;
                cp.root->last = curr;
                return curr;
            }
            decltype(auto) await_resume() {
                assert(curr.done());
                auto& p = curr.promise();
                p.root->last = p.prev;
                if constexpr (std::is_same_v<void, R>) return;
                else return *p.r;
            }
            H curr;
        };
        Awaiter operator co_await() const& noexcept { return {coro}; }

        decltype(auto) Result() const { return *coro.promise().r; }
        std::variant<int, void*> const& YieldValue() const { return coro.promise().y; }
        std::variant<int, void*>& YieldValue() { return coro.promise().y; }

        template<bool runOnce = false>
        XX_FORCE_INLINE void Run() {
            auto& p = coro.promise();
            auto& c = p.last;
            while(c && !c.done()) {
                c.resume();
                if constexpr(runOnce) return;
            }
        }
        operator bool() const { return /*!coro ||*/ coro.done(); }
        void operator()() { Run<true>(); }
        bool Resume() { Run<true>(); return *this; }
    };

    template<typename R>
    struct IsPod<Task<R>> : std::true_type {};

    /*************************************************************************************************************************/
    /*************************************************************************************************************************/

    template<typename WeakType>
    struct TasksBase {
        ListLink<std::pair<WeakType, Task<>>, int32_t> tasks;
    };

    template<>
    struct TasksBase<void> {
        ListLink<Task<>, int32_t> tasks;
    };

    template<typename WeakType>
    struct Tasks_ : TasksBase<WeakType> {
        Tasks_(Tasks_ const&) = delete;
        Tasks_& operator=(Tasks_ const&) = delete;
        Tasks_(Tasks_&&) noexcept = default;
        Tasks_& operator=(Tasks_&&) noexcept = default;
        explicit Tasks_(int32_t cap = 8) {
            this->tasks.Reserve(cap);
        }

        template<typename WT, typename CT>
        void Add(WT&& w, CT&& c) {
            if (!w || c) return;
            this->tasks.Emplace(std::pair<WeakType, Task<>> { std::forward<WT>(w), std::forward<CT>(c) });
        }

        template<typename WT, typename F>
        void AddLambda(WT&& w, F&& f) {
            Add(std::forward<WT>(w), [](F f)->Task<> {
                co_await f();
            }(std::forward<F>(f)));
        }

        template<typename CT>
        void Add(CT&& c) {
            if (c) return;
            if constexpr(IsOptional_v<WeakType>) {
                this->tasks.Emplace(std::pair<WeakType, Task<>> { WeakType{}, std::forward<CT>(c) });
            } else {
                this->tasks.Emplace(std::forward<CT>(c));
            }
        }

        template<typename F>
        void AddLambda(F&& f) {
            Add([](F f)->Task<> {
                co_await f();
            }(std::forward<F>(f)));
        }

        void Clear() {
            this->tasks.Clear();
        }

        int32_t operator()() {
            int prev = -1, next{};
            for (auto idx = this->tasks.head; idx != -1;) {
                auto& o = this->tasks[idx];
                bool needRemove;
                if constexpr(std::is_void_v<WeakType>) {
                    needRemove = o.Resume();
                } else if constexpr(IsOptional_v<WeakType>) {
                    if (o.first.has_value()) {
                        needRemove = !o.first.value() || o.second.Resume();
                    } else {
                        needRemove = o.second.Resume();
                    }
                } else {
                    needRemove = !o.first || o.second.Resume();
                }
                if (needRemove) {
                    next = this->tasks.Remove(idx, prev);
                } else {
                    next = this->tasks.Next(idx);
                    prev = idx;
                }
                idx = next;
            }
            return this->tasks.Count();
        }

        [[nodiscard]] int32_t Count() const {
            return this->tasks.Count();
        }

        [[nodiscard]] bool Empty() const {
            return !this->tasks.Count();
        }

        void Reserve(int32_t cap) {
            this->tasks.Reserve(cap);
        }
    };

    using Tasks = Tasks_<void>;

    template<typename WeakType>
    using CondTasks = Tasks_<WeakType>;

    using WeakTasks = Tasks_<WeakHolder>;

    using OptWeakTasks = Tasks_<std::optional<WeakHolder>>;

    /*************************************************************************************************************************/
    /*************************************************************************************************************************/

    template<typename KeyType = int, int timeoutSecs = 15>
    struct EventTasks {
        using YieldArgs = std::pair<KeyType, void*>;
        using Tuple = std::tuple<KeyType, void*, double, Task<>>;
        List<Tuple, int> eventTasks;
        List<Task<>, int> tasks;

        template<std::convertible_to<Task<>> T>
        void Add(T&& c) {
            if (c) return;
            auto& y = c.coro.promise().y;
            if (y.index() == 0) {
                tasks.Emplace(std::forward<T>(c));
            } else {
                auto yt = (YieldArgs*)std::get<1>(y);
                eventTasks.Emplace(yt->first, yt->second, NowSteadyEpochSeconds() + timeoutSecs, std::forward<T>(c) );
            };
        }

        template<typename F>
        void AddLambda(F&& f) {
            Add([](F f)->Task<> {
                co_await f();
            }(std::forward<F>(f)));
        }

        // match key & handle args & resume coro
        // void(*Handler)( void* ) = [???](auto p) { *(T*)p = ???; }
        // return 0: miss or success
        template<typename Handler>
        int operator()(KeyType const& v, Handler&& h) {
            for (int i = eventTasks.len - 1; i >= 0; --i) {
                auto& t = eventTasks[i];
                if (v == std::get<0>(t)) {
                    h(std::get<1>(t));
                    return Resume(i, t);
                }
            }
            return 0;
        }

        // handle eventTasks timeout & resume tasks
        // return 0: success
        int operator()() {
            if (!eventTasks.Empty()) {
                auto now = NowSteadyEpochSeconds();
                for (int i = eventTasks.len - 1; i >= 0; --i) {
                    auto& t = eventTasks[i];
                    if (std::get<2>(t) < now) {
                        if (int r = Resume(i, t)) return r;
                    }
                }
            }
            if (!tasks.Empty()) {
                for (int i = tasks.len - 1; i >= 0; --i) {
                    if (auto& c = tasks[i]; c.Resume()) {
                        tasks.SwapRemoveAt(i);
                    } else {
                        auto& y = c.coro.promise().y;
                        if (y.index() != 0) {
                            auto yt = (YieldArgs*)std::get<1>(y);
                            eventTasks.Emplace(Tuple{ yt->first, yt->second, NowSteadyEpochSeconds() + timeoutSecs, std::move(c) });
                            tasks.SwapRemoveAt(i);
                        } else {
                            if (auto& r = std::get<0>(y)) return r;
                        }
                    }
                }
            }
            return 0;
        }

        operator bool() const {
            return eventTasks.len || tasks.len;
        }

    protected:
        XX_FORCE_INLINE int Resume(int i, Tuple& t) {
            auto& c = std::get<3>(t);
            if (c.Resume()) {
                eventTasks.SwapRemoveAt(i);  // done
            } else {
                auto& y = c.coro.promise().y;
                if (y.index() == 1) {           // renew
                    auto yt = (YieldArgs*)std::get<1>(y);
                    std::get<0>(t) = yt->first;
                    std::get<1>(t) = yt->second;
                    std::get<2>(t) = NowSteadyEpochSeconds() + timeoutSecs;
                } else {
                    if (auto& r = std::get<int>(y)) return r;   // yield error number ( != 0 )
                    tasks.Emplace(std::move(c));      // yield 0
                    eventTasks.SwapRemoveAt(i);
                }
            }
            return 0;
        }
    };
}
