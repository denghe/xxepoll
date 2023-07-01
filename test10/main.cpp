// test rpc like based on pkg

// todo: CoroCache

#include "main.h"

namespace xx {

    template<typename DataType, typename KeyType, int timeoutSecs, ABComparer<DataType, KeyType> Comparer>
    struct FuncCache {
        using Callback = std::function<int(DataType const&)>;
        using Value = std::tuple<KeyType, double, Callback>;
        xx::List<Value, int> values;

        // CB example: [](DataType const& d)->int { ... }
        template<typename K, typename CB, class = std::enable_if_t<std::convertible_to<CB, Callback>>>
        void Add(K&& key, CB&& cb) {
            values.Add(std::tuple<KeyType, double, Callback>{
                std::forward<K>(key), xx::NowSteadyEpochSeconds() + timeoutSecs, std::forward<CB>(cb) } );
        }

        // handle data
        // null: dismatch     0: success      !0: error ( need close ? )
        std::optional<int> Call(DataType const& d) {
            if (values.Empty()) return false;
            for (int i = values.len - 1; i >= 0; --i) {
                auto &tup = values[i];
                if (Comparer(d, std::get<KeyType>(tup))) {
                    if (int r = std::get<Callback>(tup)(d)) return r;
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
        ~FuncCache() {
            if (values.Empty()) return;
            for (int i = values.len - 1; i >= 0; --i) {
                std::get<Callback>(values[i])({});
            }
        }
    };
}

bool DataIdComparor(xx::Data_r const& dr, uint8_t const& id) {
    return dr[dr.offset] == id;
}

int main() {
    xx::FuncCache<xx::Data_r, uint8_t, 2, DataIdComparor> fc;
    fc.Add(1, [](xx::Data_r const& d)->int {
        xx::CoutN("1 d = ", d);
        return 0;
    });
    fc.Add(5, [](xx::Data_r const& d)->int {
        xx::CoutN("5 d = ", d);
        return 0;
    });
    xx::CoutN("add. fc.values.len == ", fc.values.len);
    fc.Update();
    xx::CoutN("update. fc.values.len == ", fc.values.len);
    auto d = xx::Data::From({2,1,2});
    fc.Call({d.buf, d.len, 1});
    xx::CoutN("call. fc.values.len == ", fc.values.len);
    Sleep(3000);
    fc.Update();
    xx::CoutN("sleep + update. fc.values.len == ", fc.values.len);
    return 0;
}
