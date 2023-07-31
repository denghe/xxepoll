// event tasks test
#include "main.h"

int main() {
    auto f = []()->xx::Task<> {
        std::cout << "f 1" << std::endl;
        co_yield 1;
        std::cout << "f 2" << std::endl;
        co_yield 2.1;
        std::cout << "f 3" << std::endl;
        int i = 3;
        co_yield {123, &i};
        std::cout << "f 4 i = " << i << std::endl;
        co_return;
    };
    xx::EventTasks<> tasks;
    tasks.Add(f());
    int i = 1;
    for (; i <= 5; ++i) {
        std::cout << "step: " << i << std::endl;
        tasks();
    }
    tasks(123, [](auto p) {
        *(int*)p *= 2;
    });
    for (; i <= 7; ++i) {
        std::cout << "step: " << i << std::endl;
        tasks();
    }
    std::cout << "end" << std::endl;
    return 0;
}

//int main() {
//    auto f = []()->xx::Task<>{
//        std::cout << "a" << std::endl;
//        co_yield 1;
//        std::cout << "b" << std::endl;
//        co_yield 2.1;
//        std::cout << "c" << std::endl;
//        int i = 3;
//        co_yield &i;
//        std::cout << "i = " << i << std::endl;
//        std::cout << "d" << std::endl;
//        co_return;
//    }();
//    while(!f.Resume()) {
//        switch(f.YieldValue().index()) {
//            case 0:
//                std::cout << "y = " << std::get<0>(f.YieldValue()) << std::endl;
//                break;
//            case 1:
//                std::cout << "y = (void*)"<< (size_t)std::get<1>(f.YieldValue()) << std::endl;
//                *(int*)std::get<1>(f.YieldValue()) *= 2;
//                break;
//            default:
//                xx_assert(false);
//        }
//    }
//    std::cout << "end" << std::endl;
//    return 0;
//}
