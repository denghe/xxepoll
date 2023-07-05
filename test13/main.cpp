// test coroutine await

#include "main.h"

//https://github.com/jbaldwin/libcoro/blob/main/inc/coro/task.hpp

namespace coro {
    template<typename R = void>
    class Task;

    namespace detail {
        struct promise_base {
            struct final_awaitable {
                bool await_ready() const noexcept { return false; }
                template<typename promise_type>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h_) noexcept {
                    if (auto& p = h_.promise(); p.h) return p.h;
                    else return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            std::suspend_always initial_suspend() { return {}; }
            final_awaitable final_suspend() noexcept(true) { return {}; }
            void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
            std::coroutine_handle<> h;
        };

        template<typename R> struct promise final : promise_base {
            using Task_t = Task<R>;
            using H = std::coroutine_handle<promise<R>>;
            static constexpr bool rIsRef = std::is_lvalue_reference_v<R>;

            Task_t get_return_object() noexcept;
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

        template<> struct promise<void> : public promise_base {
            using Task_t = Task<void>;
            using H = std::coroutine_handle<promise<void>>;
            Task_t get_return_object() noexcept;
            void return_void() noexcept {}
        };
    } // namespace detail

    template<typename R>
    struct [[nodiscard]] Task {
        using promise_type     = detail::promise<R>;
        using H = std::coroutine_handle<promise_type>;

        struct awaitable_base {
            awaitable_base(H h_) noexcept : h(h_) {}
            bool await_ready() const noexcept { return !h || h.done(); }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> h_) noexcept {
                h.promise().h = h_;
                return h;
            }
            H h;
        };

        Task() noexcept = default;
        explicit Task(H h_) : h(h_) {}
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        Task(Task&& o) noexcept : h(std::exchange(o.h, nullptr)) {}
        Task& operator=(Task&& o) noexcept {
            if (std::addressof(o) != this) {
                if (h) {
                    h.destroy();
                }
                h = std::exchange(o.h, nullptr);
            }
            return *this;
        }
        ~Task() {
            if (h) {
                h.destroy();
            }
        }

        // True if the Task is in its final suspend or if the Task has been destroyed.
        bool IsReady() const noexcept { return !h || h.done(); }

        bool Resume() {
            if (!h.done()) {
                h.resume();
            }
            return !h.done();
        }

        bool Destroy() {
            if (h) {
                h.destroy();
                h = nullptr;
                return true;
            }
            return false;
        }

        auto Result() ->decltype(auto) {
            return h.promise().Result();
        }

        auto operator co_await() const& noexcept {
            struct awaitable : public awaitable_base {
                auto await_resume() -> decltype(auto) {
                    if constexpr (std::is_same_v<void, R>) return;
                    else return this->h.promise().Result();
                }
            };
            return awaitable{h};
        }
        auto operator co_await() const&& noexcept {
            struct awaitable : public awaitable_base {
                auto await_resume() -> decltype(auto) {
                    if constexpr (std::is_same_v<void, R>) return;
                    else return std::move(this->h.promise()).Result();
                }
            };
            return awaitable{h};
        }
        H h;
    };

    namespace detail {
        template<typename R>
        inline Task<R> promise<R>::get_return_object() noexcept {
            return Task<R>{H::from_promise(*this)};
        }

        inline Task<> promise<void>::get_return_object() noexcept {
            return Task<>{H::from_promise(*this)};
        }
    } // namespace detail
} // namespace coro

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
    void await_suspend(std::coroutine_handle<> h) noexcept {
        std::cout << "in Yield()\n";
        tm.coros.push(h);
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
    tm.coros.push(t.h);
    tm();
    xx::CoutN("r = ", t.Result());
    return 0;
}
