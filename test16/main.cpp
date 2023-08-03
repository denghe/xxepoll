// minor tests ( need boost )

#include "main.h"
#include <random>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ranked_index.hpp>
using boost::multi_index_container;
using namespace boost::multi_index;

namespace tags {
    struct id {};
    struct gold {};
}

struct Foo {
    int id;
    int64_t gold;
};

typedef multi_index_container<Foo, indexed_by<
                                   hashed_unique<tag<tags::id>, BOOST_MULTI_INDEX_MEMBER(Foo, int, id)>,
ranked_non_unique<tag<tags::gold>, BOOST_MULTI_INDEX_MEMBER(Foo, int64_t, gold)>
>> Foos;

int main() {
    std::mt19937 rnd(std::random_device{}());
    std::uniform_int_distribution<int64_t> goldGen(0, 100000000);

    int n = 1000000;

    std::vector<Foo> users;
    users.reserve(n);
    for (int i = 0; i < n; ++i) {
        users.emplace_back(Foo{i, goldGen(rnd)});
    }

    Foos ranks;
    ranks.reserve(n);

    auto tp = std::chrono::steady_clock::now();
    for (auto& user : users) {
        ranks.insert(user);
    }

    std::cout << "insert ms = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tp).count() << std::endl;

    auto&& ranksId = ranks.get<tags::id>();
    auto&& ranksGold = ranks.get<tags::gold>();

    tp = std::chrono::steady_clock::now();

    int64_t gold_sum = 0;
    for (int i = 0; i < n; ++i) {
        auto iter = ranksGold.nth(i);
        gold_sum += iter->gold;
    }

    std::cout << "get foo by nth( rank ) ms = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tp).count() << std::endl;
    std::cout << "gold_sum = " << gold_sum << std::endl;

    tp = std::chrono::steady_clock::now();

    int64_t rank_sum = 0;
    for (int id = 0; id < n; ++id) {
        rank_sum += ranksGold.rank(ranks.project<tags::gold>(ranksId.find(id)));
    }

    std::cout << "calc rank by id n times ms = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tp).count() << std::endl;
    std::cout << "rank_sum = " << rank_sum << std::endl;

    rank_sum = 0;
    tp = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
        auto iter = ranksId.find(9999);
        ranks.modify(iter, [&](auto& o) { o.gold = goldGen(rnd); });
        rank_sum += ranksGold.rank(ranks.project<tags::gold>(iter));
    }

    std::cout << "modify + calc rank 1 user gold n times ms = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tp).count() << std::endl;
    std::cout << "rank_sum = " << rank_sum << std::endl;

    for (int i = 0; i < 10; ++i) {
        auto iter = ranksId.find(9999);
        ranks.modify(iter, [&](auto& o) { o.gold = goldGen(rnd); });
        std::cout << "id = " << iter->id << ", gold = " << iter->gold << ", rank = " << ranksGold.rank(ranks.project<tags::gold>(iter)) << std::endl;
    }

    for (int id = 0; id < 10; ++id) {
        auto iter = ranksId.find(id);
        std::cout << "id = " << iter->id << ", gold = " << iter->gold << ", rank = " << ranksGold.rank(ranks.project<tags::gold>(iter)) << std::endl;
    }

    return 0;
}

//#include "../../robin-map/include/tsl/robin_map.h"
//#include "../..//home/xx/Desktop/hopscotch-map/include/tsl/hopscotch_map.h"
//
//template<typename Key, typename Value>
//struct hyper_map
//{
//    using key_type = Key;
//    using value_type = Value;
//
//    auto find(key_type key)
//    {
//        if (auto iter = index_.find(key); iter != index_.end())
//        {
//            return data.begin() + iter->second;
//        }
//        return data.end();
//    }
//
//    std::vector<value_type> data;
//    std::unordered_map<key_type, size_t> index_;
//};
//
//struct Foo {
//    int key;
//    std::array<char, 120> dummy;
//};
//
//int main() {
//    int n = 10000, m = 1000;
//    for (int k = 0; k < 10; ++k) {
//        {
//            xx::Dict<int, Foo> foos;
//            foos.Reserve(n);
//            for (int i = 0; i < n; ++i) {
//                foos.Add(i, Foo{i});
//            }
//            auto secs = xx::NowSteadyEpochSeconds();
//            int64_t c = 0;
//            for (int j = 0; j < m; ++j) {
//                for (int i = 0; i < n; ++i) {
//                    if (auto idx = foos.Find(i); idx != -1) {
//                        c += foos.ValueAt(idx).key;
//                    }
//                }
//            }
//            std::cout << "Dict Find c = " << c << " secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//            for (int j = 0; j < m; ++j) {
//                for (auto &kv: foos) {
//                    c += kv.value.key;
//                }
//            }
//            std::cout << "Dict for c = " << c << " secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//        }
//        {
//            hyper_map<int, Foo> foos;
//            foos.data.reserve(n);
//            for (int i = 0; i < n; ++i) {
//                foos.index_.insert({i, foos.data.size()});
//                foos.data.push_back(Foo{i});
//            }
//            auto secs = xx::NowSteadyEpochSeconds();
//            int64_t c = 0;
//            for (int j = 0; j < m; ++j) {
//                for (int i = 0; i < n; ++i) {
//                    if (auto iter = foos.find(i); iter != foos.data.end()) {
//                        c += iter->key;
//                    }
//                }
//            }
//            std::cout << "hyper_map c = " << c << " secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//            auto& data = foos.data;
//            for (int j = 0; j < m; ++j) {
//                for (auto &f : data) {
//                    c += f.key;
//                }
//            }
//            std::cout << "hyper_map for c = " << c << " secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//        }
//        {
//            tsl::robin_map<int, Foo> foos;
//            for (int i = 0; i < n; ++i) {
//                foos[i].key = i;
//            }
//            auto secs = xx::NowSteadyEpochSeconds();
//            int64_t c = 0;
//            for (int j = 0; j < m; ++j) {
//                for (int i = 0; i < n; ++i) {
//                    if (auto iter = foos.find(i); iter != foos.end()) {
//                        c += iter->second.key;
//                    }
//                }
//            }
//            std::cout << "tsl::robin_map find c = " << c << " secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//            for (int j = 0; j < m; ++j) {
//                for (auto &kv: foos) {
//                    c += kv.second.key;
//                }
//            }
//            std::cout << "tsl::robin_map for c = " << c << " secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//        }
//        {
//            tsl::hopscotch_map<int, Foo> foos;
//            for (int i = 0; i < n; ++i) {
//                foos[i].key = i;
//            }
//            auto secs = xx::NowSteadyEpochSeconds();
//            int64_t c = 0;
//            for (int j = 0; j < m; ++j) {
//                for (int i = 0; i < n; ++i) {
//                    if (auto iter = foos.find(i); iter != foos.end()) {
//                        c += iter->second.key;
//                    }
//                }
//            }
//            std::cout << "tsl::hopscotch_map find c = " << c << " secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//            for (int j = 0; j < m; ++j) {
//                for (auto &kv: foos) {
//                    c += kv.second.key;
//                }
//            }
//            std::cout << "tsl::hopscotch_map for c = " << c << " secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//        }
//        std::cout << std::endl;
////        {
////            std::vector<Foo> foos;
////            foos.resize(n);
////            for (int i = 0; i < n; ++i) {
////                foos[i].key = i;
////            }
////            auto secs = xx::NowSteadyEpochSeconds();
////            int64_t c = 0;
////            for (int j = 0; j < m; ++j) {
////                for (int i = 0; i < n; ++i) {
////                    auto iter = std::find_if(foos.begin(), foos.end(), [&](auto &o) -> bool {
////                        return o.key == i;
////                    });
////                    if (iter != foos.end()) {
////                        c += iter->key;
////                    }
////                }
////            }
////            std::cout << "Vect c = " << c << " secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
////        }
//    }
//    return 0;
//}


//struct Foo;
//using Foos = xx::Dict<int, xx::Shared<Foo>>;
//using FoosIter = int;
//
//struct Foo {
//    int key{};
//    FoosIter iter;
//};
//
//int main() {
//    Foos foos;
//    std::vector<FoosIter> iters;
//    for (int j = 0; j < 10; ++j) {
//        auto secs = xx::NowSteadyEpochSeconds();
//        for (int i = 0; i < 10000000; ++i) {
//            auto r = foos.Add(i, xx::Make<Foo>( i ));
//            foos[i]->iter = r.index;
//            iters.push_back(r.index);
//        }
//        for (auto& iter : iters) {
//            foos.RemoveAt(iter);
//        }
//        iters.clear();
//        std::cout << "secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//    }
//    std::cout << iters.size() << " " << foos.Count() << std::endl;
//    return 0;
//}


//struct Foo;
//using Foos = std::unordered_map<int, xx::Shared<Foo>>;
//using FoosIter = Foos::iterator;
//
//struct Foo {
//    int key{};
//    FoosIter iter;
//};
//
//int main() {
//    Foos foos;
//    std::vector<FoosIter> iters;
//    for (int j = 0; j < 10; ++j) {
//        auto secs = xx::NowSteadyEpochSeconds();
//        for (int i = 0; i < 10000000; ++i) {
//            auto r = foos.insert({ i, xx::Make<Foo>( i ) });
//            foos[i]->iter = r.first;
//            iters.push_back(r.first);
//        }
//        for (auto& iter : iters) {
//            foos.erase(iter);
//        }
//        iters.clear();
//        std::cout << "secs = " << xx::NowSteadyEpochSeconds(secs) << std::endl;
//    }
//    std::cout << iters.size() << " " << foos.size() << std::endl;
//    return 0;
//}
