// minor
#include "main.h"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>
using boost::multi_index_container;
using namespace boost::multi_index;

struct ByPlayerId {};
struct ByServerId {};
struct ByPlayerIdServerId {};

using Int64Pair = std::pair<int64_t, int64_t>;
struct Foo {
    int64_t playerId;
    int64_t serverId;
    [[nodiscard]] Int64Pair playerIdServerId() const {
        return {playerId, serverId};
    };
};

typedef multi_index_container<xx::Shared<Foo>, indexed_by<
    ordered_non_unique<tag<ByPlayerId>, BOOST_MULTI_INDEX_MEMBER(Foo, int64_t, playerId)>,
    ordered_non_unique<tag<ByServerId>, BOOST_MULTI_INDEX_MEMBER(Foo, int64_t, serverId)>,
    ordered_non_unique<tag<ByPlayerIdServerId>, BOOST_MULTI_INDEX_CONST_MEM_FUN(Foo, Int64Pair, playerIdServerId)>
>> SharedFoos;

int main() {
    int n = 100000, m = 10, s = 50, playerQPS = 1, serverQPS = 10;
    SharedFoos foos;

    auto&& foosPlayerId = foos.get<ByPlayerId>();
    auto&& foosServerId = foos.get<ByServerId>();
    auto&& foosPlayerIdServerId = foos.get<ByPlayerIdServerId>();

    //std::vector<Foo const*> results;
    //results.reserve(m);
    size_t results{};
    size_t resultsSize{};

    int64_t serverId{};
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            foos.emplace(xx::Make<Foo>(i, serverId));
            if (++serverId >= s) {
                serverId = 0;
            }
        }
    }
    std::cout << "foos.size() == " << foos.size() << std::endl;

    // find player by id
    auto secs = xx::NowSteadyEpochSeconds();
    for (int j = 0; j < playerQPS; ++j) {
        for (int i = 0; i < n; ++i) {
            auto [iter, end] = foosPlayerId.equal_range(i);
            //results.clear();
            results = {};
            resultsSize = {};
            for (; iter != end; ++iter) {
                //results.push_back(&*iter);
                results += (size_t)iter->pointer;
                ++resultsSize;
            }
        }
    }
    std::cout << "search playerId " << (playerQPS * n) << " times. secs = " << xx::NowSteadyEpochSeconds(secs) << " results == " << results/*.size()*/ << " resultsSize == " << resultsSize << std::endl;
    for (int j = 0; j < serverQPS; ++j) {
        for (int i = 0; i < s; ++i) {
            auto [iter, end] = foosServerId.equal_range(i);
            //results.clear();
            results = {};
            resultsSize = {};
            for (; iter != end; ++iter) {
                //results.push_back(&*iter);
                results += (size_t)iter->pointer;
                ++resultsSize;
            }
        }
    }
    std::cout << "search serverId " << (serverQPS * s) << " times. secs = " << xx::NowSteadyEpochSeconds(secs) << " results == " << results/*.size()*/ << " resultsSize == " << resultsSize << std::endl;
    for (int i = 0; i < n; ++i) {
        auto k = i % s;
        auto [iter, end] = foosPlayerIdServerId.equal_range(Int64Pair{i, k});
        //results.clear();
        results = {};
        resultsSize = {};
        for (; iter != end; ++iter) {
            //results.push_back(&*iter);
            results += (size_t)iter->pointer;
            ++resultsSize;
        }
    }
    std::cout << "search playerId + serverId " << n << " times. secs = " << xx::NowSteadyEpochSeconds(secs) << " results == " << results/*.size()*/ << " resultsSize == " << resultsSize << std::endl;
    return 0;
}
