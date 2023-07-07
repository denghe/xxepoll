// test coroutine await

#include "main.h"

// https://en.cppreference.com/w/cpp/coroutine/noop_coroutine

#include <coroutine>
#include <iostream>
#include <utility>

struct TaskBase {
    std::coroutine_handle<> coro;
    TaskBase() = delete;
    TaskBase(std::coroutine_handle<> h) {
        coro = h;
        //std::cout << "Task(h) coro = " << (size_t)coro.address() << std::endl;
    }
    TaskBase(TaskBase const& o) = delete;
    TaskBase& operator=(TaskBase const& o) = delete;
    TaskBase(TaskBase&& o) : coro(std::exchange(o.coro, nullptr)) {}
    TaskBase& operator=(TaskBase&& o) = delete;
    ~TaskBase() {
        if (coro) {
            //std::cout << "~Task() coro = " << (size_t) coro.address() << std::endl;
            coro.destroy();
        }
    }
};

template<typename T> concept IsTask = requires(T t) { std::is_base_of_v<TaskBase, T>; };

struct TaskManager;
struct PromiseBase {
    std::coroutine_handle<> previous;
    void unhandled_exception() { throw; }
};

template<class T>
struct Task : TaskBase {
    using TaskBase::TaskBase;

    struct promise_type : PromiseBase {
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
        void return_value(T value) { result = std::move(value); }

        T result;

    };
    using H = std::coroutine_handle<promise_type>;

    struct awaiter {
        bool await_ready() noexcept { return false; }
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
    TaskStore& operator=(TaskStore && o) {
        std::swap(store, o.store);
        std::swap(Deleter, o.Deleter);
        return *this;
    }

    ~TaskStore() {
        if (Deleter) {
            Deleter((void *) &store);
        }
    }

    template<IsTask T>
    TaskStore(T && o) {
        using U = std::decay_t<T>;
        Deleter = [](void* p) {
            ((U*)p)->~U();
        };
        new (&store) U(std::move(o));
    }

    std::coroutine_handle<>& Coro() {
        assert(Deleter);
        return ((TaskBase&)store).coro;
    }
};


struct TaskManager {
    xx::ListDoubleLink<TaskStore, int, uint> tasks;
    std::vector<std::coroutine_handle<>> coros, coros_;

protected:
    template<IsTask T>
    Task<int> RootTask(T t, xx::ListDoubleLinkIndexAndVersion<int, uint> iv) {
        co_await t;
        tasks.Remove(iv);
    };
public:

    template<IsTask T>
    void AddTask(T&& v) {
        auto c = (typename T::H&)v.coro;
        c.resume();
        if (!c.done()) {
            tasks.Add()( RootTask(std::forward<T>(v), tasks.Tail()) );
        }
    }

    size_t RunOnce() {
        if (coros.empty()) return 0;
        std::swap(coros, coros_);
        for(auto& c : coros_) {
            //std::cout << "RunOnce c = " << (size_t)c.address() << std::endl;
            c();
            assert(!c.done());
        }
        coros_.clear();
        return coros.size();
    }

    struct Yield_t {
        TaskManager& tm;
        Yield_t(TaskManager& tm) : tm(tm) {}
        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> coro) noexcept {
            //std::cout << "in Yield() coro = " << (size_t)coro.address() << std::endl;
            tm.coros.push_back(coro);
        };
        void await_resume() noexcept {}
    };
    auto Yield() {
        return Yield_t(*this);
    }
};

struct Foo {
    int id{};
    std::string name;
    // ...
    TaskManager tm; // last member

    Task<int> get2() {
        auto sg = xx::MakeSimpleScopeGuard([]{ std::cout << "~get2()\n"; });
        std::cout << "in get2()\n";
        co_await tm.Yield();
        std::cout << "in get2() after yield 1\n";
        co_await tm.Yield();
        std::cout << "in get2() after yield 2\n";
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
    foo.tm.AddTask(foo.test());
    foo.tm.RunOnce();
    //while(foo.tm.RunOnce());
    std::cout << "end\n";
}
