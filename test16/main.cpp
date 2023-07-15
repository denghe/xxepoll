// minor
#include "main.h"

int main() {
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
