// EventCoro

#include "main.h"

using ECoros = xx::EventCoros<int, xx::Data_r>;
using ECoro = ECoros::CoroType;
using EArgs = ECoros::Args;

struct Foo {
    Foo() {
        coros.Add(DoSth());
    }
    ECoros coros;
    ECoro DoSth() {
        xx::Data_r dr;
        co_yield EArgs{123, dr };
        xx::CoutN(dr);
        CoYield;
        co_yield EArgs{321, dr };
        xx::CoutN(dr);
    }
};

int main() {
    Foo foo;
    auto d = xx::Data::From({1,2,3});
    foo.coros.Update();
    foo.coros.Trigger(1, d);
    foo.coros.Update();
    foo.coros.Trigger(123, d);
    foo.coros.Update();
    d.Fill({3,2,1});
    foo.coros.Trigger(3, d);
    foo.coros.Update();
    foo.coros.Trigger(321, d);
    foo.coros.Update();
    return 0;
}
