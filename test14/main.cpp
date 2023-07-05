// test coroutine await

#include "main.h"

// https://en.cppreference.com/w/cpp/coroutine/noop_coroutine

#include <coroutine>
#include <iostream>
#include <utility>

template<class T>
struct task {
    struct promise_type {
        auto get_return_object() {
            return task(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() { return {}; }
        struct final_awaiter {
            bool await_ready() noexcept { return false; }
            void await_resume() noexcept {}
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (auto p = h.promise().previous; p) return p;
                else return std::noop_coroutine();
            }
        };
        final_awaiter final_suspend() noexcept { return {}; }
        void unhandled_exception() { throw; }
        void return_value(T value) { result = std::move(value); }

        T result;
        std::coroutine_handle<> previous;
    };

    task(std::coroutine_handle<promise_type> h) : coro(h) {}
    task(task&& t) = delete;
    ~task() { coro.destroy(); }

    struct awaiter {
        bool await_ready() { return false; }
        T await_resume() { return std::move(coro.promise().result); }
        auto await_suspend(std::coroutine_handle<> h) {
            coro.promise().previous = h;
            return coro;
        }
        std::coroutine_handle<promise_type> coro;
    };
    awaiter operator co_await() { return awaiter{coro}; }
    T operator()() {
        coro.resume();
        return std::move(coro.promise().result);
    }
    operator bool() const {
        return coro && !coro.done();
    }

//private:
    std::coroutine_handle<promise_type> coro;
};


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
        std::cout << "in Yield() t.mcoros.size = " << tm.coros.size() << std::endl;
        tm.coros.push(h);
    };
    void await_resume() noexcept {}
};

task<int> get_random() {
    std::cout << "in get_random()\n";
    co_await Yield();
    co_return 4;
}

task<int> test() {
    task<int> v = get_random();
    task<int> u = get_random();
    std::cout << "in test()\n";
    int x = (co_await v + co_await u);
    co_return x;
}

int main() {
    task<int> t = test();
    tm.coros.push(t.coro);
    tm();
    std::cout << "t.coro.promise().result = " << t.coro.promise().result << std::endl;
}
