// weak tasks test
#include "main.h"

struct Foo {
    ~Foo() {
        std::cout << "~Foo()\n";
    }
    xx::Task<> Update() {
        for (int i = 0; i < 10; ++i) {
            std::cout << i << std::endl;
            co_yield 0;
        }
    }
};

int main() {
    auto foo = xx::Make<Foo>();
    {
        xx::OptWeakTasks wt;
        wt.Add([]()->xx::Task<>{
            for (int i = 0;; ++i) {
                std::cout << "step " << i << std::endl;
                co_yield 0;
            }
        }); // always resume
        wt.AddTask(foo, foo->Update()); // if (weak foo) resume
        wt();
        wt();
        foo.Reset();    // kill foo immediately
        wt();
    }
    std::cout << "end\n";
    return 0;
}
