// FuncCache

#include "main.h"

namespace xx {

    template<typename KeyType, typename DataType, int timeoutSecs = 15>
    struct FuncCache {
        using Callback = std::function<int(DataType)>;
        xx::List<std::tuple<KeyType, double, Callback>, int> values;

        // CB example: [](DataType const& d)->int { ... }
        template<typename K, typename CB, class = std::enable_if_t<std::convertible_to<CB, Callback>>>
        void Add(K&& key, CB&& cb) {
            values.Add(std::tuple<KeyType, double, Callback>{
                    std::forward<K>(key), xx::NowSteadyEpochSeconds() + timeoutSecs, std::forward<CB>(cb) } );
        }

        // match key & call func
        // null: dismatch     0: success      !0: error ( need close ? )
        template<typename DT, class = std::enable_if_t<std::convertible_to<DT, DataType>>>
        std::optional<int> Trigger(KeyType const& k, DT&& d) {
            if (values.Empty()) return false;
            for (int i = values.len - 1; i >= 0; --i) {
                auto &tup = values[i];
                if (k == std::get<KeyType>(tup)) {
                    if (int r = std::get<Callback>(tup)(std::forward<DT>(d))) return r;
                    values.SwapRemoveAt(i);
                    return 0;
                }
            }
            return {};
        }

        // handle timeout
        // 0: success      !0: error ( need close ? )
        int Update() {
            if (values.Empty()) return 0;
            auto now = xx::NowSteadyEpochSeconds();
            for (int i = values.len - 1; i >= 0; --i) {
                auto &tup = values[i];
                if (std::get<double>(tup) < now) {
                    if (int r = std::get<Callback>(tup)({})) return r;
                    values.SwapRemoveAt(i);
                }
            }
            return 0;
        }

        // all timeout
        void AllTimeout() {
            if (values.Empty()) return;
            for (int i = values.len - 1; i >= 0; --i) {
                std::get<Callback>(values[i])({});
            }
        }
    };
}

int main() {
    xx::FuncCache<uint8_t, xx::Data_r, 2> fc;
    fc.Add(1, [](xx::Data_r d)->int {
        xx::CoutN("1 d = ", d);
        return 0;
    });
    fc.Add(5, [](xx::Data_r d)->int {
        xx::CoutN("5 d = ", d);
        return 0;
    });
    xx::CoutN("add. fc.values.len == ", fc.values.len);
    fc.Update();
    xx::CoutN("update. fc.values.len == ", fc.values.len);
    auto d = xx::Data::From({2,1,2});
    fc.Trigger(1, d);
    xx::CoutN("call. fc.values.len == ", fc.values.len);
    Sleep(3000);
    fc.Update();
    xx::CoutN("sleep + update. fc.values.len == ", fc.values.len);
    return 0;
}
