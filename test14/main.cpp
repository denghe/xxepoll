// test coroutine await
#include "main.h"

template<typename R>
struct Coro_;

template<>
struct Coro_<void> {
    struct promise_type {
        Coro_ get_return_object() { return { H::from_promise(*this) }; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept(true) { return {}; }
        template<typename U> std::suspend_always yield_value(U&& v) { return {}; }
        void return_void() {}
        void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
    };
    using H = std::coroutine_handle<promise_type>;

    constexpr bool await_ready() const noexcept { return false; }
    template<typename AH>
    void await_suspend(AH ah) {
        xx::CoutN("Coro_void> ah = ", (size_t)ah.address());
        xx::CoutN("Coro_void> h = ", (size_t)h.address());
    }
    constexpr void await_resume() const noexcept {}

    H h;
    Coro_() : h(nullptr) {}
    Coro_(H h) : h(h) {}
    ~Coro_() { if (h) h.destroy(); }
    Coro_(Coro_ const& o) = delete;
    Coro_& operator=(Coro_ const&) = delete;
    Coro_(Coro_&& o) noexcept : h(o.h) { o.h = nullptr; };
    Coro_& operator=(Coro_&& o) noexcept { std::swap(h, o.h); return *this; };

    void operator()() { h.resume(); }
    [[nodiscard]] bool Done() const { return h.done(); }
    bool Resume() { h.resume(); return h.done(); }
};
using Coro = Coro_<void>;

template<typename R>
struct Coro_ {
    struct promise_type {
        R r;
        Coro_ get_return_object() { return { H::from_promise(*this) }; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept(true) { return {}; }
        template<typename U>
        void return_value(U&& u) {
            r = std::forward<U>(u);
            xx::CoutN("u = ", u);
        }
        void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
    };
    using H = std::coroutine_handle<promise_type>;
    H h;

    constexpr bool await_ready() const noexcept { return false; }
    template<typename AH>
    void await_suspend(AH ah) {
        xx::CoutN("ah = ", (size_t)ah.address());
        xx::CoutN("h = ", (size_t)h.address());
    }
    [[nodiscard]] constexpr R await_resume() const noexcept {
        return h.promise().r;
    }

    Coro_() : h(nullptr) {}
    Coro_(H h) : h(h) {}
    ~Coro_() { if (h) h.destroy(); }
    Coro_(Coro_ const& o) = delete;
    Coro_& operator=(Coro_ const&) = delete;
    Coro_(Coro_&& o) noexcept : h(o.h) { o.h = nullptr; };
    Coro_& operator=(Coro_&& o) noexcept { std::swap(h, o.h); return *this; };

    void operator()() { h.resume(); }
    [[nodiscard]] bool Done() const { return h.done(); }
    bool Resume() { h.resume(); return h.done(); }
};
using Coro = Coro_<void>;

Coro_<int> A(int n) {
    co_await std::suspend_always{};
    co_return n + n;
}

Coro B() {
    auto a = co_await A(1);
    xx::CoutN(a);
}

int main() {
    auto c = B();
    for (int i = 0; i < 999; ++i) {
        xx::CoutN("i = ", i );
        if (c.Resume()) break;
    }
    return 0;
}
