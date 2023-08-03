// test Task without manager 1
#include "main.h"

int main() {
    auto t = []()->xx::Task<int>{
        std::cout << "before co_yield 1" << std::endl;
        co_yield 1;
        std::cout << "before co_yield 2" << std::endl;
        co_yield 2;
        std::cout << "before co_return 123" << std::endl;
        co_return 123;
    }();
    for (int i = 0;; ++i) {
        if (t.Resume()) break;
        std::cout << "i = " << i << std::endl;
    }
    std::cout << "t = " << t.Result() << std::endl;
    return 0;
}

/*

// old design life cycle is terrible
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

// new design life cycle is good
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
