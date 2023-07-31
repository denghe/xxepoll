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
        wt.AddLambda([]()->xx::Task<>{
            for (int i = 0;; ++i) {
                std::cout << "step " << i << std::endl;
                co_yield 0;
            }
        }); // always resume
        wt.Add(foo, foo->Update()); // if (weak foo) resume
        wt();
        wt();
        foo.Reset();    // kill foo immediately
        wt();
    }
    std::cout << "end\n";
    return 0;
}

//int main() {
//    auto i = xx::Make<int>();
//    {
//        xx::WeakTasks wt;
//        wt.AddLambda(i.ToWeak(), [&i = *i]() -> xx::Task<> {
//            auto sg = xx::MakeSimpleScopeGuard([] {
//                std::cout << "lambda is dead\n";
//            });
//            while (true) {
//                ++i;
//                std::cout << i << std::endl;
//                co_yield 0;
//            }
//        });
//        std::cout << "before call wt 1\n";
//        wt();   // output 1
//        std::cout << "before call wt 2\n";
//        wt();   // output 2
//        i.Reset();
//        std::cout << "before call wt 3\n";
//        wt();   // no output
//    }
//    std::cout << "end\n";
//    return 0;
//}
