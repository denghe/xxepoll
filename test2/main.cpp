// test Task without manager 2
#include "main.h"

struct Foo {
    int n = 0;
    xx::Task<int> get2() {
        co_yield 1;
        co_return 4;
    }
    xx::Task<int> get1() {
        co_return co_await get2() * co_await get2();
    }
    xx::Task<> test() {
        auto v = co_await get1();
        n += v;
    }
};

int main() {
    for (int j = 0; j < 10; ++j) {
        Foo f;
        auto secs = xx::NowSteadyEpochSeconds();
        for (int i = 0; i < 10000000; ++i) {
            f.test().Run();
        }
        std::cout << "foo.n = " << f.n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
    }
}
