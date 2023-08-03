// test Tasks
#include "main.h"

namespace xx {
    using IterTasks_t = ListDoubleLink<xx::Task<>, int32_t, uint32_t>;
    struct IterTasks;
    struct IterTasksItemDeleter {
        IterTasks_t* tasks;
        IterTasks_t::IndexAndVersion iv;
        // todo
        ~IterTasksItemDeleter();
    };

    struct IterTasks {
        IterTasks_t tasks;

        IterTasks(IterTasks const &) = delete;
        IterTasks &operator=(IterTasks const &) = delete;
        IterTasks(IterTasks &&) noexcept = default;
        IterTasks &operator=(IterTasks &&) noexcept = default;
        explicit IterTasks(int32_t cap = 8) {
            this->tasks.Reserve(cap);
        }

        template<typename T>
        IterTasksItemDeleter AddTask(T &&t) {
            if (t) return {};
            tasks.Emplace(std::forward<T>(t));
            return { &tasks, tasks.Tail() };
        }

        template<typename F>
        IterTasksItemDeleter Add(F &&f) {
            return AddTask([](F f) -> Task<> {
                co_await f();
            }(std::forward<F>(f)));
        }

        void Clear() {
            this->tasks.Clear();
        }

        int32_t operator()() {
            tasks.Foreach([&](auto& o)->bool {
                return o.Resume();
            });
            return tasks.Count();
        }

        [[nodiscard]] int32_t Count() const {
            return this->tasks.Count();
        }

        [[nodiscard]] bool Empty() const {
            return !this->tasks.Count();
        }

        void Reserve(int32_t cap) {
            this->tasks.Reserve(cap);
        }
    };

    inline IterTasksItemDeleter::~IterTasksItemDeleter() {
        if (tasks) {
            tasks->Remove(iv);
            tasks = {};
        }
    }
}

struct Foo {
    xx::IterTasksItemDeleter d;
    int n = 0;
    xx::Task<int> Get2() {
        co_yield 1;
        co_return 4;
    }
    xx::Task<int> Get1() {
        co_return co_await Get2() * co_await Get2();
    }
    xx::Task<> Get0() {
        auto v = co_await Get1();
        n += v;
    }
    void Go(xx::IterTasks& tasks) {
        d = tasks.AddTask(Get0());
    }
};

int main() {
    xx::IterTasks tasks;
    Foo f;
    f.Go(tasks);
    while (tasks());
    std::cout << f.n << std::endl;
}
