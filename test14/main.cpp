// test Task without manager
#include "main.h"

struct Foo {
    int n = 0;
    xx::Task<int> get2() {
        co_yield 1;
        co_return 4;    // todo: fix
    }
    xx::Task<int> get1() {
        co_return co_await get2() * co_await get2();
    }
    xx::Task<> test() {
        auto v = co_await get1();
        n += v;
    }
};

int main() {
    for (int j = 0; j < 10; ++j) {
        Foo f;
        auto secs = xx::NowSteadyEpochSeconds();
        for (int i = 0; i < 10000000; ++i) {
            f.test()();
        }
        std::cout << "foo.n = " << f.n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";
    }
}


//int main() {
//    auto t = []()->Task<int>{
//        std::cout << "before co_yield 1" << std::endl;
//        co_yield 1;
//        std::cout << "before co_yield 2" << std::endl;
//        co_yield 2;
//        std::cout << "before co_return 123" << std::endl;
//        co_return 123;
//    }();
//    for (int i = 0;; ++i) {
//        if (t()) break;
//        std::cout << "i = " << i << std::endl;
//    }
//    std::cout << "t = " << t.Result() << std::endl;
//    return 0;
//}







/*
struct monster {
    bool alive = true;
    awaitable update(std::shared_ptr<monster> memholder) {
        while( alive ) {
            co_await yield{};
        }
    }
};
struct player {
    std::weak_ptr<monster> target;
};
int main() {
    auto m = std::make_shared<monster>();
    co_spawn( m->update(m) );
    auto p = std::make_shared<player>();
    p->target = m;
    // ...
    m.alive = false;
    m.reset();
    // ... wait m coroutine quit? m dispose?
    if (auto m = p->target.lock()) {
        if (m->alive) {
            // ...
        }
    }
}





struct monster {
    task_mgr tm;
    awaitable update() {
        while( true ) {
            co_await yield{};
        }
    }
};
struct player {
    std::weak_ptr<monster> target;
};
int main() {
    auto m = std::make_shared<monster>();
    m->tm.co_spawn( m->update() );
    auto p = std::make_shared<player>();
    p->target = m;
    // ...
    m.reset();
    if (auto m = p->target.lock()) {
        // ...
    }
}
*/
