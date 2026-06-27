#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    memkit::MovingAverage<std::int32_t, 4> avg;
    assert(avg.empty());

    assert(memkit::ok(avg.push(10)));
    assert(memkit::ok(avg.push(20)));
    assert(memkit::ok(avg.push(30)));
    assert(avg.average() == 20);

    assert(memkit::ok(avg.push(40)));
    assert(avg.full());
    assert(avg.average() == 25);

    assert(memkit::ok(avg.push(50)));
    assert(avg.average() == 35);

    avg.clear();
    assert(avg.empty());

    memkit::MovingAverage<float, 3> favg;
    assert(memkit::ok(favg.push(1.0f)));
    assert(memkit::ok(favg.push(2.0f)));
    assert(memkit::ok(favg.push(3.0f)));
    assert(favg.average() == 2.0f);

    memkit::WindowStats<std::int32_t, 4> stats;
    assert(memkit::ok(stats.push(10)));
    assert(memkit::ok(stats.push(30)));
    assert(memkit::ok(stats.push(20)));
    assert(stats.min() == 10);
    assert(stats.max() == 30);
    assert(stats.average() == 20);

    std::printf("running_stats: ok\n");
    return 0;
}
