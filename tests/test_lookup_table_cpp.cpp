#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    const std::int32_t keys[]   = {0, 10, 20, 30};
    const std::int32_t values[] = {100, 200, 300, 400};

    memkit::LookupTable<std::int32_t, std::int32_t> table;
    assert(memkit::ok(table.init(keys, values, 4u, memkit::lookup_mode::interpolate)));
    assert(table.at(-5) == 100);
    assert(table.at(35) == 400);
    assert(table.at(15) == 250);
    assert(table.at(10) == 200);

    const float fkeys[]   = {0.0f, 1.0f, 2.0f};
    const float fvalues[] = {0.0f, 10.0f, 30.0f};
    memkit::LookupTable<float, float> curve;
    assert(memkit::ok(curve.init(fkeys, fvalues, 3u)));
    assert(curve.at(0.5f) == 5.0f);

    memkit::LookupTable<std::int32_t, std::int32_t> nearest;
    assert(memkit::ok(nearest.init(keys, values, 4u, memkit::lookup_mode::nearest)));
    assert(nearest.at(14) == 200);
    assert(nearest.at(16) == 300);

    std::int32_t exact = 0;
    memkit::LookupTable<std::int32_t, std::int32_t> exact_table;
    assert(memkit::ok(exact_table.init(keys, values, 4u, memkit::lookup_mode::exact)));
    assert(memkit::ok(exact_table.lookup(20, exact)));
    assert(exact == 300);
    assert(exact_table.lookup(15, exact) == memkit::status::not_found);

    std::printf("lookup_table: ok\n");
    return 0;
}
