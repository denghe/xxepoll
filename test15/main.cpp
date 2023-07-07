// test Task

#include "main.h"

struct Foo {
    int id{};
    std::string name;
    // ...
    XX_TASKMANAGER_MEMBER_ATTACH;

    xx::Task<int> get2() {
        auto sg = xx::MakeSimpleScopeGuard([]{ std::cout << "~get2()\n"; });
        std::cout << "in get2()\n";
        XxTaskYield;
        std::cout << "in get2() after yield 1\n";
        XxTaskYield;
        std::cout << "in get2() after yield 2\n";
        co_return 4;
    }

    xx::Task<int> get1() {
        auto sg = xx::MakeSimpleScopeGuard([]{ std::cout << "~get1()\n"; });
        std::cout << "in get1()\n";
        co_return co_await get2() * co_await get2();
    }

    xx::Task<> test() {
        auto sg = xx::MakeSimpleScopeGuard([]{ std::cout << "~test()\n"; });
        std::cout << "in test()\n";
        auto v = co_await get1();
        std::cout << "get1() = " << v << std::endl;
    }
};

int main() {
    Foo foo;
    foo.AddTask(foo.test());
    while(foo.RunOnce());
    std::cout << "end\n";
}
