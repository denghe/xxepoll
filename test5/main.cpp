#include "main.h"

struct CoroBase {
    xx::Coros coros;
};

struct Context {
    std::unordered_set<xx::Weak<CoroBase>> cbs;
    void AddCoro(xx::Shared<CoroBase> tar, xx::Coro coro) {
        tar->coros.Add(std::move(coro));
        cbs.insert(tar.ToWeak());
    }
    size_t Run() {
        for (auto&& iter = cbs.begin(); iter != cbs.end();) {
            if (iter->h->sharedCount) {
                (*iter)->coros();
                ++iter;
            } else {
                iter = cbs.erase(iter);
            }
        }
        return cbs.size();
    }
};

struct A : CoroBase {
    xx::Coro xxx() {
        for (int i = 0; i < 3; ++i) {
            std::cout << "i = " << i << std::endl;
            CoYield;
        }
    }
};

int main() {
    Context ctx;
    {
        auto a = xx::Make<A>();
        ctx.AddCoro(a, a->xxx());
    }
    while (ctx.Run());
    return 0;
}
