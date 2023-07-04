// test coroutine await
#include "main.h"

https://github.com/jbaldwin/libcoro/blob/main/inc/coro/task.hpp


//struct Task {
//    // The coroutine level type
//    struct promise_type {
//        Task get_return_object() { return { Handle::from_promise(*this) }; }
//        std::suspend_always initial_suspend() { return {}; }
//        std::suspend_never final_suspend() noexcept { return {}; }
//        void return_void() { }
//        void unhandled_exception() { }
//    };
//    using Handle = std::coroutine_handle<promise_type>;
//    Handle coro_;
//    void destroy() { coro_.destroy(); }
//    void resume() { coro_.resume(); }
//};
//
//Task myCoroutine() {
//    co_return; // make it a coroutine
//}
//int main() {
//    auto c = myCoroutine();
//    c.resume();
//    // c.destroy();
//}




//#include <coroutine>
//#include <iostream>
//#include <cassert>
//
//struct HelloWorldCoro {
//    struct promise_type {
//        int m_value;
//
//        HelloWorldCoro get_return_object() { return this; }
//        std::suspend_always initial_suspend() { return {}; }
//        std::suspend_always final_suspend() noexcept { return {}; }
//        void unhandled_exception() {}
//        void return_value(int val) { m_value = val; }
//    };
//
//    HelloWorldCoro(promise_type* p) : m_handle(std::coroutine_handle<promise_type>::from_promise(*p)) {}
//    ~HelloWorldCoro() { m_handle.destroy(); }
//
//    std::coroutine_handle<promise_type>      m_handle;
//};
//
//HelloWorldCoro print_hello_world() {
//    std::cout << "Hello ";
//    co_await std::suspend_always{ };
//    std::cout << "World!" << std::endl;
//
//    co_return -1;
//}
//
//
//int main() {
//    auto mycoro = print_hello_world();
//
//    mycoro.m_handle.resume();
//    mycoro.m_handle.resume();
//    assert(mycoro.m_handle.promise().m_value == -1);
//    return EXIT_SUCCESS;
//}


//
//template<typename PromiseType>
//struct GetPromise {
//    PromiseType *p_;
//    bool await_ready() { return false; } // says yes call await_suspend
//    bool await_suspend(std::coroutine_handle<PromiseType> h) {
//        p_ = &h.promise();
//        return false;     // says no don't suspend coroutine after all
//    }
//    PromiseType *await_resume() { return p_; }
//};
//
//struct ReturnObject3 {
//    struct promise_type {
//        unsigned value_;
//        ReturnObject3 get_return_object() { return { H::from_promise(*this) }; }
//        std::suspend_never initial_suspend() { return {}; }
//        std::suspend_never final_suspend() noexcept { return {}; }
//        void unhandled_exception() {}
//    };
//    using H = std::coroutine_handle<promise_type>;
//    H h_;
//};
//
//ReturnObject3 counter3() {
//    auto pp = co_await GetPromise<ReturnObject3::promise_type>{};
//    for (unsigned i = 0;; ++i) {
//        pp->value_ = i;
//        co_await std::suspend_always{};
//    }
//}
//
//int main() {
//    auto h = counter3().h_;
//    auto &promise = h.promise();
//    for (int i = 0; i < 3; ++i) {
//        std::cout << "counter3: " << promise.value_ << std::endl;
//        h();
//    }
//    h.destroy();
//    return 0;
//}