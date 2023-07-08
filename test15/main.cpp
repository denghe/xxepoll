// test Task Manager
#include "main.h"

struct Foo {
    int n = 0;
    xx::Task<> testCoro1() {
        co_yield 0;
        n += 1;
        co_yield 0;
        n += 1;
    }
    xx::Coro testCoro2() {
        co_yield 0;
        n += 1;
        co_yield 0;
        n += 1;
    }
};
int main() {
    for (int j = 0; j < 10; ++j) {
        Foo f;
        auto secs = xx::NowSteadyEpochSeconds();
        for (int i = 0; i < 10000000; ++i) {
            auto o = f.testCoro1();
            o.Run();
        }
        std::cout << "testCoro1 foo.n = " << f.n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
    }
    for (int j = 0; j < 10; ++j) {
        Foo f;
        auto secs = xx::NowSteadyEpochSeconds();
        for (int i = 0; i < 10000000; ++i) {
            auto o = f.testCoro2();
            o.Run();
        }
        std::cout << "testCoro2 foo.n = " << f.n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
    }
}
