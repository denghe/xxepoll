#include "main.h"

struct Foo {
    xx::Task<> DoSth() {
        std::cout << "yield 0" << std::endl;
        co_yield 0;
        std::cout << "yield 1" << std::endl;
        co_yield 1;
        std::cout << "end do sth" << std::endl;
    }
};
struct FooContainer {
    xx::CondTasks<xx::Weak<Foo>> tasks;
    xx::Shared<Foo> foo;
};

int main() {
    FooContainer fc;
    fc.foo.Emplace();
    fc.tasks.Add(fc.foo.ToWeak(), fc.foo->DoSth());
    fc.tasks();
    fc.tasks();
    fc.foo.Reset(); // no end do sth output
    fc.tasks();
    return 0;
}
