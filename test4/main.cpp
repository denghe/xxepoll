// event tasks test
#include "main.h"

int main() {
    auto f = []()->xx::Task<> {
        std::cout << "f 1" << std::endl;
        co_yield 1;
        std::cout << "f 2" << std::endl;
        co_yield 2;
        std::cout << "f 3" << std::endl;
        int i = 3;
        co_yield {123, &i};
        std::cout << "f 4 i = " << i << std::endl;
        co_return;
    };
    xx::EventTasks<> tasks;
    tasks.AddTask(f());
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
