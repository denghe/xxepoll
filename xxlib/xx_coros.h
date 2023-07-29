﻿#pragma once

#include "xx_coro_simple.h"
#include "xx_task.h"

#include "xx_typetraits.h"
#include "xx_time.h"
#include "xx_list.h"
#include "xx_listlink.h"

namespace xx {

    template<typename CoroType, typename WeakType>
    struct CorosBase {
        xx::ListLink<std::pair<WeakType, CoroType>, int32_t> coros;
    };

    template<typename CoroType>
    struct CorosBase<CoroType, void> {
        xx::ListLink<CoroType, int32_t> coros;
    };

    template<typename CoroType, typename WeakType>
    struct Coros_ : CorosBase<CoroType, WeakType> {
        Coros_(Coros_ const&) = delete;
        Coros_& operator=(Coros_ const&) = delete;
        Coros_(Coros_&&) noexcept = default;
        Coros_& operator=(Coros_&&) noexcept = default;
        explicit Coros_(int32_t cap = 8) {
            this->coros.Reserve(cap);
        }

        template<typename WT, typename CT>
        void Add(WT&& w, CT&& c) {
            if (!w || c) return;
            this->coros.Emplace(std::pair<WeakType, CoroType> { std::forward<WT>(w), std::forward<CT>(c) });
        }

        template<typename CT>
        void Add(CT&& c) {
            if (c) return;
            this->coros.Emplace(std::forward<CT>(c));
        }

        template<typename F>
        int AddLambda(F&& f) {
            return Add([](F f)->CoroType {
                co_await f();
            }(std::forward<F>(f)));
        }

        void Clear() {
            this->coros.Clear();
        }

        int32_t operator()() {
            int prev = -1, next{};
            for (auto idx = this->coros.head; idx != -1;) {
                auto& o = this->coros[idx];
                bool needRemove;
                if constexpr(std::is_void_v<WeakType>) {
                    needRemove = o.Resume();
                } else {
                    needRemove = !o.first || o.second.Resume();
                }
                if (needRemove) {
                    next = this->coros.Remove(idx, prev);
                } else {
                    next = this->coros.Next(idx);
                    prev = idx;
                }
                idx = next;
            }
            return this->coros.Count();
        }

        [[nodiscard]] int32_t Count() const {
            return this->coros.Count();
        }

        [[nodiscard]] bool Empty() const {
            return !this->coros.Count();
        }

        void Reserve(int32_t cap) {
            this->coros.Reserve(cap);
        }
    };

    using Coros = Coros_<Coro, void>;
    template<typename WeakType>
    using CondCoros = Coros_<Coro, WeakType>;  // check condition before Resume ( for life cycle manage )

    using Tasks = Coros_<Task<>, void>;
    template<typename WeakType>
    using CondTasks = Coros_<Task<>, WeakType>;


    /***********************************************************************************************************/
    /***********************************************************************************************************/

    template<typename KeyType, typename DataType>
    using EventArgs = std::pair<KeyType, DataType&>;

    template<typename KeyType, typename DataType>
    using EventYieldType = std::variant<int, EventArgs<KeyType, DataType>>;   // int for yield once

    template<typename KeyType, typename DataType>
    using EventTask = Task<DataType, EventYieldType<KeyType, DataType>>;

    template<typename KeyType, typename DataType, int timeoutSecs = 15>
    struct EventTasks {
        using TaskType = EventTask<KeyType, DataType>;
        using Args = EventArgs<KeyType, DataType>;
        using Tuple = std::tuple<Args, double, TaskType>;
        xx::List<Tuple, int> condCoros;
        xx::List<TaskType, int> updateCoros;

        template<std::convertible_to<TaskType> CT>
        int Add(CT&& c) {
            if (c) return 0;
            auto& y = c.coro.promise().y;
            if (y.index() == 0) {
                updateCoros.Emplace(std::forward<CT>(c));
            } else if (y.index() == 1) {
                condCoros.Emplace(std::move(std::get<Args>(y)), xx::NowSteadyEpochSeconds() + timeoutSecs, std::forward<CT>(c) );
            } else if (y.index() == 2) return std::get<int>(y);
            return 0;
        }

        int Add(Task<>&& c) {
            return Add([](Task<> c)->TaskType {
                co_await c;
            }(std::move(c)));
        }

        template<typename F>
        int AddLambda(F&& f) {
            return Add([](F f)->TaskType {
                co_await f();
            }(std::forward<F>(f)));
        }

        // match key & store d & resume coro
        // return 0: miss or success
        template<typename DT = DataType>
        int Trigger(KeyType const& v, DT&& d) {
            for (int i = condCoros.len - 1; i >= 0; --i) {
                auto& tup = condCoros[i];
                if (v == std::get<Args>(tup).first) {
                    std::get<Args>(tup).second = std::forward<DT>(d);
                    return Resume(i);
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
                    auto& tup = condCoros[i];
                    if (std::get<double>(tup) < now) {
                        std::get<Args>(tup).second = {};
                        if (int r = Resume(i)) return r;
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
                            condCoros.Emplace(Tuple{ std::move(std::get<Args>(y)), xx::NowSteadyEpochSeconds() + timeoutSecs, std::move(c) });
                            updateCoros.SwapRemoveAt(i);
                        } else {
                            if (auto& r = std::get<int>(y)) return r;
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
        XX_FORCE_INLINE int Resume(int i) {
            auto& tup = condCoros[i];
            auto& args = std::get<Args>(tup);
            auto& c = std::get<TaskType>(tup);
            if (c.Resume()) {
                condCoros.SwapRemoveAt(i);  // done
            } else {
                auto& y = c.coro.promise().y;
                if (y.index() == 1) {   // renew
                    args = std::move(std::get<Args>(y));
                    std::get<double>(tup) = xx::NowSteadyEpochSeconds() + timeoutSecs;
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
