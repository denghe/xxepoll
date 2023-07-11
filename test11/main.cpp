// EventCoro

#include "main.h"

using Tasks = xx::EventTasks<int, xx::Data_r>;
using Task = Tasks::CoroType;
using Args = Tasks::Args;

struct Foo {
    Foo() {
        tasks.Add(DoSth());
    }
    Tasks tasks;    // todo: constructor add tasks ?
    Task DoSth() {
        xx::Data_r dr;
        co_yield Args{ 123, dr };
        xx::CoutN(dr);
        co_yield 0;
        co_yield Args{ 321, dr };
        xx::CoutN(dr);
    }
};

int main() {
    Foo foo;
    auto d = xx::Data::From({1,2,3});
    foo.tasks();
    foo.tasks.Trigger(1, d);
    foo.tasks();
    foo.tasks.Trigger(123, d);
    foo.tasks();
    d.Fill({3,2,1});
    foo.tasks.Trigger(3, d);
    foo.tasks();
    foo.tasks.Trigger(321, d);
    foo.tasks();
    return 0;
}
