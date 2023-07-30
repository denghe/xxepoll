// more task design test
#include "main.h"

namespace xx {
    template<typename R = void>
    struct Task;

    namespace detail {
        template<typename Derived, typename R>
        struct PromiseBase {
            std::coroutine_handle<> prev, last;
            PromiseBase *root{ this };
            std::variant<int, void*> y; // any ??? more type support ?

            struct FinalAwaiter {
                bool await_ready() const noexcept { return false; }
                void await_resume() noexcept {}
                template<typename promise_type>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> curr) noexcept {
                    if (auto &p = curr.promise(); p.prev) return p.prev;
                    else return std::noop_coroutine();
                }
            };
            Task<R> get_return_object() noexcept {
                auto tmp = Task<R>{ std::coroutine_handle<Derived>::from_promise(*(Derived*)this) };
                tmp.coro.promise().last = tmp.coro;
                return tmp;
            }
            template<typename P, class = std::enable_if_t<std::is_pointer_v<P>>>
            std::suspend_always yield_value(P const& p) {
                root->y = (void*)p;
                return {};
            }
            std::suspend_always yield_value(int v) {
                root->y = v;
                return {};
            }
            std::suspend_always initial_suspend() { return {}; }
            FinalAwaiter final_suspend() noexcept(true) { return {}; }
            void unhandled_exception() { throw; }
        };

        template<typename R>
        struct Promise final : PromiseBase<Promise<R>, R> {
            std::optional<R> r;

            template<typename T>
            void return_value(T&& v) { r.emplace(std::forward<T>(v)); }
        };

        template<>
        struct Promise<void> : PromiseBase<Promise<void>, void> {
            void return_void() noexcept {}
        };
    }

    /*************************************************************************************************************************/
    /*************************************************************************************************************************/

    template<typename R>
    struct [[nodiscard]] Task {
        using promise_type = detail::Promise<R>;
        using H = std::coroutine_handle<promise_type>;
        H coro;

        Task() = delete;
        Task(H h) { coro = h; }
        Task(Task const &o) = delete;
        Task &operator=(Task const &o) = delete;
        Task(Task &&o) : coro(std::exchange(o.coro, nullptr)) {}
        Task &operator=(Task &&o) {
            std::swap(coro, o.coro);
            return *this;
        }
        ~Task() { if (coro) { coro.destroy(); } }

        struct Awaiter {
            bool await_ready() const noexcept { return false; }
            decltype(auto) await_suspend(std::coroutine_handle<> prev) noexcept {
                auto& cp = curr.promise();
                cp.prev = prev;
                cp.root = ((H&)prev).promise().root;
                cp.root->last = curr;
                return curr;
            }
            decltype(auto) await_resume() {
                assert(curr.done());
                auto& p = curr.promise();
                p.root->last = p.prev;
                if constexpr (std::is_same_v<void, R>) return;
                else return *p.r;
            }
            H curr;
        };
        Awaiter operator co_await() const& noexcept { return {coro}; }

        decltype(auto) Result() const { return *coro.promise().r; }
        std::variant<int, void*> const& YieldValue() const { return coro.promise().y; }
        std::variant<int, void*>& YieldValue() { return coro.promise().y; }

        template<bool runOnce = false>
        XX_FORCE_INLINE void Run() {
            auto& p = coro.promise();
            auto& c = p.last;
            while(c && !c.done()) {
                c.resume();
                if constexpr(runOnce) return;
            }
        }
        operator bool() const { return /*!coro ||*/ coro.done(); }
        void operator()() { Run<true>(); }
        bool Resume() { Run<true>(); return *this; }
    };

    template<typename R>
    struct IsPod<Task<R>> : std::true_type {};

    /*************************************************************************************************************************/
    /*************************************************************************************************************************/

    template<typename KeyType = int, int timeoutSecs = 15>
    struct EventTasks {
        using YieldType = std::pair<KeyType, void*>;
        using Tuple = std::tuple<KeyType, void*, double, Task<>>;
        xx::List<Tuple, int> condCoros;
        xx::List<Task<>, int> updateCoros;

        template<std::convertible_to<Task<>> T>
        void Add(T&& c) {
            if (c) return;
            auto& y = c.coro.promise().y;
            if (y.index() == 0) {
                updateCoros.Emplace(std::forward<T>(c));
            } else {
                auto yt = (YieldType*)std::get<1>(y);
                condCoros.Emplace(yt->first, yt->second, xx::NowSteadyEpochSeconds() + timeoutSecs, std::forward<T>(c) );
            };
        }

        template<typename F>
        void AddLambda(F&& f) {
            Add([](F f)->Task<> {
                co_await f();
            }(std::forward<F>(f)));
        }

        // match key & handle args & resume coro
        // void(*Handler)( void* ) = [???](auto p) { *(T*)p = ???; }
        // return 0: miss or success
        template<typename Handler>
        int operator()(KeyType const& v, Handler&& h) {
            for (int i = condCoros.len - 1; i >= 0; --i) {
                auto& t = condCoros[i];
                if (v == std::get<0>(t)) {
                    h(std::get<1>(t));
                    return Resume(i, t);
                }
            }
            return 0;
        }

        // handle condCoros timeout & resume updateCoros
        // return 0: success
        int operator()() {
            if (!condCoros.Empty()) {
                auto now = xx::NowSteadyEpochSeconds();
                for (int i = condCoros.len - 1; i >= 0; --i) {
                    auto& t = condCoros[i];
                    if (std::get<2>(t) < now) {
                        if (int r = Resume(i, t)) return r;
                    }
                }
            }
            if (!updateCoros.Empty()) {
                for (int i = updateCoros.len - 1; i >= 0; --i) {
                    if (auto& c = updateCoros[i]; c.Resume()) {
                        updateCoros.SwapRemoveAt(i);
                    } else {
                        auto& y = c.coro.promise().y;
                        if (y.index() != 0) {
                            auto yt = (YieldType*)std::get<1>(y);
                            condCoros.Emplace(Tuple{ yt->first, yt->second, xx::NowSteadyEpochSeconds() + timeoutSecs, std::move(c) });
                            updateCoros.SwapRemoveAt(i);
                        } else {
                            if (auto& r = std::get<0>(y)) return r;
                        }
                    }
                }
            }
            return 0;
        }

        operator bool() const {
            return condCoros.len || updateCoros.len;
        }

    protected:
        XX_FORCE_INLINE int Resume(int i, Tuple& t) {
            auto& c = std::get<3>(t);
            if (c.Resume()) {
                condCoros.SwapRemoveAt(i);  // done
            } else {
                auto& y = c.coro.promise().y;
                if (y.index() == 1) {           // renew
                    auto yt = (YieldType*)std::get<1>(y);
                    std::get<0>(t) = yt->first;
                    std::get<1>(t) = yt->second;
                    std::get<2>(t) = xx::NowSteadyEpochSeconds() + timeoutSecs;
                } else {
                    if (auto& r = std::get<int>(y)) return r;   // yield error number ( != 0 )
                    updateCoros.Emplace(std::move(c));      // yield 0
                    condCoros.SwapRemoveAt(i);
                }
            }
            return 0;
        }
    };
}


int main() {
    using YieldType = xx::EventTasks<>::YieldType;
    auto f = []()->xx::Task<> {
        std::cout << "a" << std::endl;
        co_yield 1;
        std::cout << "b" << std::endl;
        co_yield 2.1;
        std::cout << "c" << std::endl;
        int i = 3;
        YieldType yt{ 123, &i };
        co_yield &yt;
        std::cout << "i = " << i << std::endl;
        std::cout << "d" << std::endl;
        co_return;
    };
    xx::EventTasks<> tasks;
    tasks.Add(f());
    int i = 0;
    for (; i < 5; ++i) {
        std::cout << "step: " << i << std::endl;
        tasks();
    }
    tasks(123, [](auto p) {
        *(int*)p *= 2;
    });
    for (; i < 10; ++i) {
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
