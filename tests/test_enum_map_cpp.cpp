#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

enum class mode : std::uint8_t {
    idle  = 0,
    run   = 1,
    fault = 2,
    count = 3,
};

int main()
{
    memkit::EnumMap<mode, int, static_cast<std::size_t>(mode::count)> map;
    assert(map.empty());

    assert(memkit::ok(map.put(mode::idle, 1)));
    assert(memkit::ok(map.put(mode::run, 2)));
    assert(map.size() == 2u);

    int value = 0;
    assert(memkit::ok(map.get(mode::run, value)));
    assert(value == 2);
    assert(!map.contains(mode::fault));

    assert(memkit::ok(map.put(mode::run, 22)));
    assert(memkit::ok(map.get(mode::run, value)));
    assert(value == 22);

    int* slot = map.try_get(mode::idle);
    assert(slot != nullptr);
    assert(*slot == 1);

    assert(memkit::ok(map.remove(mode::idle)));
    assert(!map.contains(mode::idle));
    assert(map.size() == 1u);

    map.clear();
    assert(map.empty());

    std::printf("enum_map: ok\n");
    return 0;
}
