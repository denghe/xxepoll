#pragma once
// important: only support static function or lambda !!!  COPY data from arguments !!! do not ref !!!

#include "xx_typetraits.h"
namespace xx {
    template<typename R = void>
    struct Task;

    namespace detail {
        template<typename Derived, typename R>
        struct PromiseBase {
            std::coroutine_handle<> prev, last;
            PromiseBase *root{ this };

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
            std::suspend_always initial_suspend() { return {}; }
            FinalAwaiter final_suspend() noexcept(true) { return {}; }
            std::suspend_always yield_value(int) { return {}; }
            void unhandled_exception() { throw; }
        };

        template<typename R>
        struct Promise final : PromiseBase<Promise<R>, R> {
            std::optional<R> r;

            template<typename T>
            void return_value(T&& v) { r = std::forward<T>(v); }
        };

        template<>
        struct Promise<void> : PromiseBase<Promise<void>, void> {
            void return_void() noexcept {}
        };
    }

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
        template<bool runOnce>
        XX_FORCE_INLINE decltype(auto) Run() {
            auto& p = coro.promise();
            auto& c = p.last;
            while(c && !c.done()) {
                c.resume();
                if constexpr(runOnce) return;
            }
            if constexpr (!std::is_void_v<R>) return *p.r;
        }
        operator bool() const { return !coro || coro.done(); }
        void operator()() { Run<false>(); }
        bool Resume() { Run<true>(); return *this; }
    };

    template<typename T>
    struct IsPod<Task<T>> : std::true_type {};
}

