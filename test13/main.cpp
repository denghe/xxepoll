// test coroutine await

#include "main.h"

// reference: https://github.com/jbaldwin/libcoro/blob/main/inc/coro/task.hpp

namespace coro {
    template<typename R = void>
    struct Task;

    namespace detail {
        struct PromiseBase {
            struct FinalAwaiter {
                bool await_ready() const noexcept { return false; }
                void await_resume() noexcept {}
                template<typename promise_type>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> current) noexcept {
                    if (auto& p = current.promise(); p.previous) return p.previous;
                    else return std::noop_coroutine();
                }
            };
            std::suspend_always initial_suspend() { return {}; }
            FinalAwaiter final_suspend() noexcept(true) { return {}; }
            void unhandled_exception() { throw; }
            std::coroutine_handle<> previous;
        };
        template<typename R> struct Promise final : PromiseBase {
            static constexpr bool rIsRef = std::is_lvalue_reference_v<R>;
            Task<R> get_return_object() noexcept;
            void return_value(R v) {
                if constexpr (rIsRef) {
                    r.reset();
                    r = v;
                } else {
                    r = std::move(v);
                }
            }
            std::conditional_t<rIsRef, R, const R&> Result() const& {
                if constexpr (rIsRef) return r.value();
                else return r;
            }
            R&& Result() && requires(not rIsRef) {
                return std::move(r);
            }
        private:
            std::conditional_t<rIsRef, std::optional<std::reference_wrapper<std::remove_reference_t<R>>>, R> r;
        };
        template<> struct Promise<void> : PromiseBase {
            Task<void> get_return_object() noexcept;
            void return_void() noexcept {}
        };
    }

    template<typename R>
    struct [[nodiscard]] Task {
        using promise_type = detail::Promise<R>;
        using H = std::coroutine_handle<promise_type>;
        H coro;

        Task(H h_) : coro(h_) {}
        Task(Task const&) = delete;
        Task& operator=(Task const&) = delete;
        Task(Task&& o) noexcept : coro(std::exchange(o.coro, nullptr)) {}
        Task& operator=(Task&& o) noexcept {
            if (std::addressof(o) != this) {
                if (coro) {
                    coro.destroy();
                }
                coro = std::exchange(o.coro, nullptr);
            }
            return *this;
        }
        ~Task() {
            if (coro) {
                coro.destroy();
            }
        }

        struct AwaiterBase {
            bool await_ready() const noexcept { return false; }
            auto await_suspend(std::coroutine_handle<> previous) noexcept {
                current.promise().previous = previous;
                return current;
            }
            H current;
        };
        auto operator co_await() const& noexcept {
            struct Awaiter : AwaiterBase {
                auto await_resume() -> decltype(auto) {
                    if constexpr (std::is_same_v<void, R>) return;
                    else return this->current.promise().Result();
                }
            };
            return Awaiter{coro};
        }
        auto operator co_await() const&& noexcept {
            struct Awaiter : AwaiterBase {
                auto await_resume() -> decltype(auto) {
                    if constexpr (std::is_same_v<void, R>) return;
                    else return std::move(this->current.promise()).Result();
                }
            };
            return Awaiter{coro};
        }

        operator bool() const {
            assert(coro);
            return !coro.done();
        }
        void Resume() {
            coro.resume();
        }
        auto Result() const ->decltype(auto) {
            return coro.promise().Result();
        }
    };

    namespace detail {
        template<typename R>
        inline Task<R> Promise<R>::get_return_object() noexcept {
            return Task<R>{ std::coroutine_handle<Promise<R>>::from_promise(*this)};
        }

        inline Task<> Promise<void>::get_return_object() noexcept {
            return Task<>{std::coroutine_handle<Promise<void>>::from_promise(*this)};
        }
    }
}

/****************************************************************************************************/

struct taskmgr {
    std::queue<std::coroutine_handle<>> coros;
    void operator()() {
        while (!coros.empty()) {
            auto coro = coros.front();
            coros.pop();
            coro();
        }
    }
} tm;

struct Yield {
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> coro) noexcept {
        std::cout << "in Yield()\n";
        tm.coros.push(coro);
    };
    void await_resume() noexcept {}
};

coro::Task<int> calc1(int i) {
    co_await Yield();
    co_await Yield();
    co_return i + i;
}

coro::Task<int> calc2(int i) {
    co_await Yield();
    auto a = co_await calc1(i);
    co_await Yield();
    co_return a + a;
}

coro::Task<int> calc3(int i) {
    co_await Yield();
    auto a = co_await calc2(i);
    co_await Yield();
    co_return a + a;
}

int main() {
    auto t = calc3(1);
    tm.coros.push(t.coro);
    tm();
    xx::CoutN("r = ", t.Result());
    return 0;
}
