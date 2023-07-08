// test Task without manager
#include "main.h"

template<typename R = void>
struct Task;

namespace detail {
    template<typename Derived, typename R>
    struct PromiseBase {
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

        std::coroutine_handle<> prev, last;
        PromiseBase *root{ this };
    };

    template<typename R>
    struct Promise final : PromiseBase<Promise<R>, R> {
        template<typename T>
        void return_value(T&& v) { r = std::forward<T>(v); }
        std::optional<R> r;
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
    Task() = delete;
    Task(H h) { coro = h; }
    Task(Task const &o) = delete;
    Task &operator=(Task const &o) = delete;
    Task(Task &&o) : coro(std::exchange(o.coro, nullptr)) {}
    Task &operator=(Task &&o) = delete;
    ~Task() { if (coro) { coro.destroy(); } }

    template<bool needMovePromise>
    struct Awaiter {
        bool await_ready() const noexcept { return false; }
        auto await_suspend(std::coroutine_handle<> prev) noexcept {
            auto& cp = curr.promise();
            cp.prev = prev;
            cp.root = ((H&)prev).promise().root;
            cp.root->last = curr;
            return curr;
        }
        auto await_resume() -> decltype(auto) {
            assert(curr.done());
            auto& p = curr.promise();
            p.root->last = p.prev;
            if constexpr (std::is_same_v<void, R>) return;
            else if constexpr (needMovePromise) return *std::move(p).r;
            else return *p.r;
        }
        H curr;
    };
    auto operator co_await() const& noexcept { return Awaiter<false>{coro}; }
    auto operator co_await() const&& noexcept { return Awaiter<true>{coro}; }

    decltype(auto) operator()() {
        auto& p = coro.promise();
        auto& c = p.last;
        while(c && !c.done()) {
            c.resume();
        }
        if constexpr (!std::is_void_v<R>) return *p.r;
    }
    bool IsReady() const { return !coro || coro.done(); }
    decltype(auto) Result() const { return *coro.promise().r; }
    void Resume() {
        auto& c = coro.promise().last;
        while(c && !c.done()) {
            c.resume();
        }
    }
    H coro;
};


struct Foo {
    int n = 0;
    Task<int> get2() {
        co_yield 1;
        co_return 4;    // todo: fix
    }
    Task<int> get1() {
        co_return co_await get2() * co_await get2();
    }
    Task<> test() {
        auto v = co_await get1();
        n += v;
    }
};

int main() {
    for (int j = 0; j < 10; ++j) {
        Foo f;
        auto secs = xx::NowSteadyEpochSeconds();
        for (int i = 0; i < 10000000; ++i) {
            auto t = f.test();
            t();
        }
        std::cout << "foo.n = " << f.n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
    }
}


//int main() {
//    auto t = []()->Task<int>{
//        std::cout << "before co_yield 1" << std::endl;
//        co_yield 1;
//        std::cout << "before co_yield 2" << std::endl;
//        co_yield 2;
//        std::cout << "before co_return 123" << std::endl;
//        co_return 123;
//    }();
//    for (int i = 0;; ++i) {
//        if (t()) break;
//        std::cout << "i = " << i << std::endl;
//    }
//    std::cout << "t = " << t.Result() << std::endl;
//    return 0;
//}







/*
struct monster {
    bool alive = true;
    awaitable update(std::shared_ptr<monster> memholder) {
        while( alive ) {
            co_await yield{};
        }
    }
};
struct player {
    std::weak_ptr<monster> target;
};
int main() {
    auto m = std::make_shared<monster>();
    co_spawn( m->update(m) );
    auto p = std::make_shared<player>();
    p->target = m;
    // ...
    m.alive = false;
    m.reset();
    // ... wait m coroutine quit? m dispose?
    if (auto m = p->target.lock()) {
        if (m->alive) {
            // ...
        }
    }
}





struct monster {
    task_mgr tm;
    awaitable update() {
        while( true ) {
            co_await yield{};
        }
    }
};
struct player {
    std::weak_ptr<monster> target;
};
int main() {
    auto m = std::make_shared<monster>();
    m->tm.co_spawn( m->update() );
    auto p = std::make_shared<player>();
    p->target = m;
    // ...
    m.reset();
    if (auto m = p->target.lock()) {
        // ...
    }
}
*/
