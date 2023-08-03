// test Task manager performance
#include "main.h"

xx::Task<int64_t> func1(int64_t a) {
    co_yield 0;
    co_return a + a;
}
xx::Task<int64_t> func2(int64_t b) {
    co_return co_await func1(b) + co_await func1(b);
}

int main() {
    xx::Tasks tasks;
    auto s = "asdfasdfasdfasdfasdfasdfasdfasdfasdfasdfasdfasdf"s;
    tasks.Add([&]()->xx::Task<>{
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
