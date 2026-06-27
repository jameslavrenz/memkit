#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    memkit::stl::array<std::byte, memkit::FlatMap<int, int>::storage_bytes<8>()> backing{};
    memkit::FlatMap<int, int> map;
    assert(memkit::ok(map.init(backing.data(), 8u)));

    assert(map.empty());
    assert(memkit::ok(map.put(30, 300)));
    assert(memkit::ok(map.put(10, 100)));
    assert(memkit::ok(map.put(20, 200)));
    assert(map.size() == 3u);

    int value = 0;
    assert(memkit::ok(map.get(10, value)));
    assert(value == 100);
    assert(memkit::ok(map.get(20, value)));
    assert(value == 200);
    assert(memkit::ok(map.get(30, value)));
    assert(value == 300);
    assert(!map.contains(99));

    assert(memkit::ok(map.put(20, 222)));
    assert(memkit::ok(map.get(20, value)));
    assert(value == 222);

    assert(memkit::ok(map.remove(10)));
    assert(!map.contains(10));
    assert(map.size() == 2u);

    assert(memkit::ok(map.remove(30, value)));
    assert(value == 300);
    assert(map.size() == 1u);

    memkit::stl::array<std::byte, 512> arena_backing{};
    memkit::memory::fixed_buffer buf{arena_backing};
    memkit::memory::static_arena arena{buf};

    memkit::FlatMap<int, int> arena_map;
    assert(memkit::ok(arena_map.init_from_arena(arena, 4u)));
    assert(memkit::ok(arena_map.put(1, 10)));
    assert(memkit::ok(arena_map.get(1, value)));
    assert(value == 10);

    std::printf("flat_map: ok\n");
    return 0;
}
