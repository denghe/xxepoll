#pragma once

// important: only support static function or lambda !!!  COPY data from arguments !!! do not ref !!!

#include "xx_typetraits.h"
#include "xx_time.h"
#include "xx_list.h"
#include "xx_listlink.h"
#include "xx_listdoublelink.h"

namespace xx {

    namespace detail {
        struct TaskBase {
            std::coroutine_handle<> coro;

            TaskBase() = delete;
            TaskBase(std::coroutine_handle<> h) {
                coro = h;
            }
            TaskBase(TaskBase const &o) = delete;
            TaskBase &operator=(TaskBase const &o) = delete;
            TaskBase(TaskBase &&o) : coro(std::exchange(o.coro, nullptr)) {}
            TaskBase &operator=(TaskBase &&o) = delete;
            ~TaskBase() {
                if (coro) {
                    coro.destroy();
                }
            }
        };

        template<typename T> concept IsTask = requires(T t) { std::is_base_of_v<TaskBase, T>; };

        struct TaskStore {
            std::aligned_storage_t<sizeof(TaskBase), sizeof(TaskBase)> store;

            void (*Deleter)(void *){};
            TaskStore() = default;
            TaskStore(TaskStore const &) = delete;
            TaskStore &operator=(TaskStore const &) = delete;
            TaskStore(TaskStore &&o) : store(o.store), Deleter(std::exchange(o.Deleter, nullptr)) {}

            TaskStore &operator=(TaskStore &&o) {
                std::swap(store, o.store);
                std::swap(Deleter, o.Deleter);
                return *this;
            }

            ~TaskStore() {
                if (Deleter) {
                    Deleter((void *) &store);
                }
            }

            template<IsTask T>
            TaskStore(T &&o) {
                using U = std::decay_t<T>;
                Deleter = [](void *p) {
                    ((U *) p)->~U();
                };
                new(&store) U(std::move(o));
            }

            std::coroutine_handle<> &Coro() {
                assert(Deleter);
                return ((TaskBase &) store).coro;
            }
        };
    }

    template<typename R = void>
    struct Task;

    namespace detail {
        struct PromiseBase {
            struct FinalAwaiter {
                bool await_ready() const noexcept { return false; }
                void await_resume() noexcept {}

                template<typename promise_type>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> current) noexcept {
                    if (auto &p = current.promise(); p.previous) return p.previous;
                    else return std::noop_coroutine();
                }
            };

            std::suspend_always initial_suspend() { return {}; }
            FinalAwaiter final_suspend() noexcept(true) { return {}; }
            void unhandled_exception() { throw; }

            std::coroutine_handle<> previous;
        };

        template<typename R>
        struct Promise final : PromiseBase {
            static constexpr bool rIsRef = std::is_lvalue_reference_v<R>;

            Task<R> get_return_object() noexcept;

            void return_value(R v) {
                if constexpr (rIsRef) {
                    r.reset();
                    r = v;
                } else {
                    r = std::move(v);
                }
            }

            std::conditional_t<rIsRef, R, const R &> Result() const &{
                if constexpr (rIsRef) return r.value();
                else return r;
            }

            R &&Result() &&requires(not rIsRef) {
                return std::move(r);
            }

        private:
            std::conditional_t<rIsRef, std::optional<std::reference_wrapper<std::remove_reference_t<R>>>, R> r;
        };

        template<>
        struct Promise<void> : PromiseBase {
            Task<void> get_return_object() noexcept;

            void return_void() noexcept {}
        };
    }

    template<typename R>
    struct [[nodiscard]] Task : detail::TaskBase {
        using detail::TaskBase::TaskBase;
        using promise_type = detail::Promise<R>;
        using H = std::coroutine_handle<promise_type>;

        struct AwaiterBase {
            bool await_ready() const noexcept { return false; }

            auto await_suspend(std::coroutine_handle<> previous) noexcept {
                current.promise().previous = previous;
                return current;
            }

            H current;
        };

        auto operator
        co_await() const& noexcept {
            struct Awaiter : AwaiterBase {
                auto await_resume() -> decltype(auto) {
                    if constexpr (std::is_same_v<void, R>) return;
                    else return this->current.promise().Result();
                }
            };
            return Awaiter{(H &) coro};
        }

        auto operator
        co_await() const&& noexcept {
            struct Awaiter : AwaiterBase {
                auto await_resume() -> decltype(auto) {
                    if constexpr (std::is_same_v<void, R>) return;
                    else return std::move(this->current.promise()).Result();
                }
            };
            return Awaiter{(H &) coro};
        }

        auto Result() const -> decltype(auto) {
            return ((H &) coro).promise().Result();
        }
    };

    namespace detail {
        template<typename R>
        inline Task<R> Promise<R>::get_return_object() noexcept {
            return Task<R>{std::coroutine_handle<Promise<R>>::from_promise(*this)};
        }

        inline Task<> Promise<void>::get_return_object() noexcept {
            return Task<>{std::coroutine_handle<Promise<void>>::from_promise(*this)};
        }
    }

    struct TaskManager {
        xx::ListDoubleLink<detail::TaskStore, int, uint> tasks;
        std::vector<std::coroutine_handle<>> coros, coros_;

    protected:
        template<detail::IsTask T>
        Task<int> RootTask(T t, xx::ListDoubleLinkIndexAndVersion<int, uint> iv) {
            co_await t;
            tasks.Remove(iv);
        };
    public:

        template<detail::IsTask T>
        void AddTask(T &&v) {
            auto c = (typename T::H &) v.coro;
            c.resume();
            if (!c.done()) {
                tasks.Add()(RootTask(std::forward<T>(v), tasks.Tail()));
            }
        }

        size_t RunOnce() {
            if (coros.empty()) return 0;
            std::swap(coros, coros_);
            for (auto &c: coros_) {
                c();
            }
            coros_.clear();
            return coros.size();
        }

        struct YieldAwaiter {
            TaskManager &tm;

            YieldAwaiter(TaskManager &tm) : tm(tm) {}
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<> coro) noexcept {
                tm.coros.push_back(coro);
            };
            void await_resume() noexcept {}
        };

        auto Yield() {
            return YieldAwaiter(*this);
        }
    };

}
