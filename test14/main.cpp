// test coroutine await

#include "main.h"

// https://en.cppreference.com/w/cpp/coroutine/noop_coroutine

#include <coroutine>
#include <iostream>
#include <utility>

struct TaskBase {
    std::coroutine_handle<> coro;
};

template<class T>
struct Task : TaskBase {
    struct promise_type {
        auto get_return_object() {
            return Task(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() { return {}; }
        struct final_awaiter {
            bool await_ready() noexcept { return false; }
            void await_resume() noexcept {}
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> curr) noexcept {
                if (auto p = curr.promise().previous; p)
                    return p;
                else return std::noop_coroutine();
            }
        };
        final_awaiter final_suspend() noexcept { return {}; }
        void unhandled_exception() { throw; }
        void return_value(T value) { result = std::move(value); }

        T result;
        std::coroutine_handle<> previous;
    };
    using H = std::coroutine_handle<promise_type>;

    Task(H h) {
        coro = h;
        std::cout << "Task(h) coro = " << (size_t)coro.address() << std::endl;
    }
    Task(Task&& t) = delete;
    ~Task() {
        std::cout << "~Task() coro = " << (size_t)coro.address() << std::endl;
        coro.destroy();
    }

    struct awaiter {
        bool await_ready() { return false; }
        T await_resume() { return std::move(curr.promise().result); }
        auto await_suspend(std::coroutine_handle<> prev) {
            curr.promise().previous = prev;
            return curr;
        }
        H curr;
    };
    awaiter operator co_await() { return awaiter{ (H&)coro }; }
};

struct TaskStore {
    std::aligned_storage_t<sizeof(TaskBase), sizeof(TaskBase)> store;
    void(*Deleter)(void*){};

    TaskStore() = default;
    TaskStore(TaskStore const&) = delete;
    TaskStore& operator=(TaskStore const&) = delete;
    TaskStore(TaskStore && o) : store(o.store), Deleter(std::exchange(o.Deleter, nullptr)) {}
    TaskStore& operator=(TaskStore && o) = delete;

    ~TaskStore() {
        Deleter((void*)&store);
    }

    template<typename T>
    void SetValue(T&& v) {
        new (&store) T(std::move(v));
        Deleter = [](void* o) {
            ((T*)o)->~T();
        };
    }

    void Resume() {
        assert(Deleter);
        ((TaskBase&)store).coro.resume();
    }

    bool Done() const {
        assert(Deleter);
        return ((TaskBase&)store).coro.done();
    }
};

//template <typename T> concept IsBool = std::is_same_v<bool, T>;
//template<typename T> concept Has_Resume = requires(T t) { t.Resume() -> IsBool; };
template<typename T> concept IsTask = requires(T t) { std::is_base_of_v<TaskBase, T>; };

struct TaskManager {
    xx::ListDoubleLink<TaskStore, int, uint> tasks;
    std::vector<std::coroutine_handle<>> coros, coros_;

    template<IsTask T>
    void Add(T&& v) {
        auto& t = tasks.Emplace();
        auto iv = tasks.Tail();
        t.SetValue(std::forward<T>(v));
        if (t.Resume(); t.Done()) {
            tasks.Remove(iv);
        }
    }

    size_t RunOnce() {
        if (coros.empty()) return 0;
        std::swap(coros, coros_);
        for(auto& c : coros_) {
            c();
        }
        coros_.clear();
        tasks.Foreach([](auto& Task)->bool {
            return Task.Done();
        });
        return coros.size();
    }

    struct Yield_t {
        TaskManager& tm;
        Yield_t(TaskManager& tm) : tm(tm) {}
        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> coro) noexcept {
            std::cout << "in Yield() coro = " << (size_t)coro.address() << std::endl;
            tm.coros.push_back(coro);
        };
        void await_resume() noexcept {}
    };
    auto Yield() {
        return Yield_t(*this);
    }

};

struct Foo {
    TaskManager tm;

    Task<int> get2() {
        auto sg = xx::MakeSimpleScopeGuard([]{ std::cout << "~get2()\n"; });
        std::cout << "in get2()\n";
        //co_await Yield();
        co_await tm.Yield();
        std::cout << "in get2() after yield\n";
        co_return 4;
    }

    Task<int> get1() {
        auto sg = xx::MakeSimpleScopeGuard([]{ std::cout << "~get1()\n"; });
        std::cout << "in get1()\n";
        auto xxx = co_await get2();
        co_return xxx;
    }

    Task<int> test() {
        auto sg = xx::MakeSimpleScopeGuard([]{ std::cout << "~test()\n"; });
        std::cout << "in test()\n";
        auto v = co_await get1();
        co_return v;
    }

};

int main() {
    Foo foo;
    foo.tm.Add(foo.test());
}
