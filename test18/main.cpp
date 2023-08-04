// test boost asio coroutine "yield" performance
#include "main.h"
#include <boost/asio.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
namespace asio = boost::asio;
namespace boostsystem = boost::system;

using namespace asio::experimental::awaitable_operators;
constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);
using asio::co_spawn;
using asio::awaitable;
using asio::detached;

awaitable<int64_t> func1(int64_t a) {
    co_await asio::post(use_nothrow_awaitable);
    co_return a + a;
}
awaitable<int64_t> func2(int64_t b) {
    co_return co_await func1(b) + co_await func1(b);
}

int main() {
    asio::io_context ioc(1);
    co_spawn(ioc, [&]()->awaitable<void> {

        auto secs = xx::NowSteadyEpochSeconds();
        int64_t n = 0;
        for (int64_t i = 0; i < 10000000; ++i) {
            n += co_await func2(i);
        }
        std::cout << "n = " << n << ", secs = " << xx::NowSteadyEpochSeconds(secs) << "\n";

    }, detached);
    ioc.run();
    return 0;
}
