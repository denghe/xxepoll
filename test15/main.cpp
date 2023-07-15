// test Task Manager
#include "main.h"

xx::Task<int64_t> func1(int64_t a) {
    co_yield 0;
    co_return a + a;
}
xx::Task<int64_t> func2(int64_t b) {
    co_return co_await func1(b) + co_await func1(b);
}

int main() {
    xx::EventTasks<int, xx::Data_r&> tasks;
    auto s = "asdfasdfasdfasdfasdfasdfasdfasdfasdfasdfasdfasdf"s;
    tasks.AddLambda([&]()->xx::Task<>{
        auto secs = xx::NowSteadyEpochSeconds();
        int64_t n = 0;
        for (int64_t i = 0; i < 10000000; ++i) {
            n += co_await func2(i);
        }
        std::cout << "n = " << n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
        std::cout << s << std::endl;
    });
    while(tasks());
    return 0;
}


//xx::Task<int64_t> func1(int64_t a) {
//    co_yield 0;
//    co_return a + a;
//}
//xx::Task<int64_t> func2(int64_t b) {
//    co_return co_await func1(b) + co_await func1(b);
//}
//
//xx::Task<> func() {
//    auto secs = xx::NowSteadyEpochSeconds();
//    int64_t n = 0;
//    for (int64_t i = 0; i < 10000000; ++i) {
//        n += co_await func2(i);
//    }
//    std::cout << "n = " << n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
//}
//
//int main() {
//    xx::Tasks tasks;  // xx::EventTasks<int, xx::Data_r&> tasks;
//    tasks.Add(func());
//    while(tasks());
//    return 0;
//}



//struct Foo {
//    int n = 0;
//    xx::Task<> testCoro1() {
//        co_yield 0;
//        n += 1;
//        co_yield 0;
//        n += 1;
//    }
//    xx::Coro testCoro2() {
//        co_yield 0;
//        n += 1;
//        co_yield 0;
//        n += 1;
//    }
//};
//
//int main() {
//    for (int j = 0; j < 10; ++j) {
//        Foo f;
//        auto secs = xx::NowSteadyEpochSeconds();
//        for (int i = 0; i < 10000000; ++i) {
//            auto o = f.testCoro1();
//            o.Run();
//        }
//        std::cout << "testCoro1 foo.n = " << f.n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
//    }
//    for (int j = 0; j < 10; ++j) {
//        Foo f;
//        auto secs = xx::NowSteadyEpochSeconds();
//        for (int i = 0; i < 10000000; ++i) {
//            auto o = f.testCoro2();
//            o.Run();
//        }
//        std::cout << "testCoro2 foo.n = " << f.n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
//    }
//}
