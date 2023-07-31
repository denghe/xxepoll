// cond tasks test
#include "main.h"

int main() {
    auto i = xx::Make<int>();
    xx::WeakTasks wt;
    wt.AddLambda(i.ToWeak(), [&i = *i]()->xx::Task<>{
        while(true) {
            ++i;
            std::cout << i << std::endl;
            co_yield 0;
        }
    });
    wt();   // output 1
    wt();   // output 2
    i.Reset();
    wt();   // no output
    std::cout << "end\n";
    return 0;
}
