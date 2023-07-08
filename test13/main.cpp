#include <coroutine>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <thread>
#include <type_traits>

template<typename T>
class TaskPromise;

template<typename T>
class Task;

template<typename T>
class TaskAwaiter {
public:
    friend Task<T>;
    bool await_ready() {
        return false;
    }

    template<typename U>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise<U>> h);
    decltype(auto)          await_resume();

private:
    TaskAwaiter(std::coroutine_handle<TaskPromise<T>> handle) : m_handle(handle) {}
    std::coroutine_handle<TaskPromise<T>> m_handle;
};

template<typename T = void>
class Task {
public:
    using promise_type = TaskPromise<T>;
    friend promise_type;

    ~Task() {
        if (m_handle && m_handle.done()) {
            m_handle.destroy();
        }
    }

    Task(Task &&other) : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    Task(const Task &) = delete;

    Task &operator=(Task &&other) {
        if (std::addressof(other) != this) {
            std::swap(m_handle, other.m_handle);
            if (other.m_handle) {
                other.m_handle.destroy();
            }
        }
        return *this;
    }
    Task &operator=(const Task &) = delete;

    TaskAwaiter<T> operator co_await() const noexcept {
        return TaskAwaiter<T>{m_handle};
    }

    bool is_ready() const noexcept {
        return !m_handle || m_handle.done();
    }

    decltype(auto) sync() {
        //std::cout<<m_handle.promise().latest.address()<<" "<<<<std::endl;
        while(m_handle.promise().latest && !m_handle.promise().latest.done()) {
            //std::cout<<m_handle.promise().latest.address()<<std::endl;
            m_handle.promise().latest.resume();
        }
        return m_handle.promise().result();
    }

    Task(std::coroutine_handle<promise_type> handle) : m_handle(handle) {}
    std::coroutine_handle<promise_type> m_handle;
};
struct final_awaitable {
    bool await_ready() const noexcept {
        return false;
    }

    template<typename PROMISE>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<PROMISE> coro) noexcept {
        return coro.promise().m_continuation == nullptr ? std::noop_coroutine() : coro.promise().m_continuation;
    }

    void await_resume() noexcept {}
};


struct TaskPromiseBaseBase {
    TaskPromiseBaseBase() {
        root_promise = this;
    };
    TaskPromiseBaseBase    *root_promise = nullptr;
    std::coroutine_handle<> latest = nullptr;
};

template<typename T>
struct TaskPromiseBase : public TaskPromiseBaseBase {

public:
    friend final_awaitable;
    friend TaskAwaiter<T>;
    std::suspend_always initial_suspend() {
        return {};
    }

    final_awaitable final_suspend() noexcept {

        return final_awaitable{};
    }

    std::suspend_always yield_value(int) {
        return {};
    }

    std::coroutine_handle<> m_continuation = nullptr;
};

template<typename T>
struct TaskPromise : public TaskPromiseBase<T> {
public:
    TaskPromise(){};

    Task<T> get_return_object() {
        auto tmp = Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
        tmp.m_handle.promise().latest = tmp.m_handle;
        return tmp;
    }

    void unhandled_exception() {
        ::new (static_cast<void *>(std::addressof(m_exception))) std::exception_ptr(std::current_exception());
        m_state = State::Exception;
    }

    ~TaskPromise() {
        if (m_state == State::Value) {
            m_value.~T();
        } else if (m_state == State::Exception) {
            m_exception.~exception_ptr();
        }
    }

    T result() {
        if (m_state == State::Value) {
            return std::move(m_value);
        } else if (m_state == State::Exception) {
            std::rethrow_exception(m_exception);
        } else {

            throw std::logic_error("Task not ready");
        }
    }

    template<typename VALUE, typename = std::enable_if_t<std::is_convertible_v<VALUE &&, T>>>
    void return_value(VALUE &&value) noexcept(std::is_nothrow_constructible_v<T, VALUE &&>) {

        ::new (static_cast<void *>(std::addressof(m_value))) T(std::forward<VALUE>(value));
        m_state = State::Value;
    }

private:
    enum class State {
        Empty,
        Value,
        Exception,
    };
    State m_state = State::Empty;

    union {
        T                  m_value;
        std::exception_ptr m_exception = nullptr;
    };
};

template<>
struct TaskPromise<void> : public TaskPromiseBase<void> {
public:
    Task<> get_return_object() {
        auto tmp = Task<>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
        tmp.m_handle.promise().latest = tmp.m_handle;
        return tmp;
    }
    void return_void() {}

    void unhandled_exception() {
        m_exception = std::current_exception();
    }

    ~TaskPromise() = default;

    void result() {
        if (m_exception)
            std::rethrow_exception(m_exception);
    }

private:
    std::exception_ptr m_exception;
};


template<typename T>
template<typename U>
std::coroutine_handle<> TaskAwaiter<T>::await_suspend(std::coroutine_handle<TaskPromise<U>> h) {

    m_handle.promise().m_continuation = h;
    m_handle.promise().root_promise = h.promise().root_promise;
    m_handle.promise().root_promise->latest = m_handle;

    //std::cout<<&m_handle.promise()<<" "<<m_handle.promise().root_promise<<std::endl;
    return m_handle;
}


template<typename T>
decltype(auto) TaskAwaiter<T>::await_resume() {

    if (!m_handle.done())
        throw std::runtime_error("Task not done");
    m_handle.promise().root_promise->latest = m_handle.promise().m_continuation;
    return m_handle.promise().result();
}


//Task<int> ttt() {
//    std::cout << "111" << std::endl;
//    co_yield 1;
//    std::cout << "222" << std::endl;
//    co_return 1;
//}
//
//Task<int> xxx() {
//    std::cout << "333" << std::endl;
//    co_return (co_await ttt()) + (co_await ttt());
//}

//int main() {
//    auto xx = xxx();
//    auto t = xx.sync();
//    std::cout<<t<<std::endl;
//}


struct Foo {
    int n = 0;
    Task<int> get2() {
        co_yield 1;
        co_return 4;
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
    Foo f;
    auto secs = xx::NowSteadyEpochSeconds();
    for (int i = 0; i < 10000000; ++i) {
        auto t = f.test();
        t.sync();
    }
    std::cout << "foo.n = " << f.n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
}
