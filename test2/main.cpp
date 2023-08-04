// test Tasks Delete
#include "main.h"

struct Foo {
    xx::TaskDeleter td;
    int n = 0;
    xx::Task<int> Get2() {
        co_yield 1;
        co_return 4;
    }
    xx::Task<int> Get1() {
        co_return co_await Get2() * co_await Get2();
    }
    xx::Task<> Get0() {
        auto sg = xx::MakeSimpleScopeGuard([]{
            std::cout << "task Get0() quit" << std::endl;
        });
        auto v = co_await Get1();
        n += v;
    }
};

int main() {
    xx::Tasks tasks;
    {
        Foo f;
        f.td(tasks, f.Get0());
        tasks();    // resume once
    }
    while (tasks());    // resume all
    std::cout << "end" << std::endl;
}
