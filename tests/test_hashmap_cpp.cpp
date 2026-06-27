#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/config.hpp>
#include <memkit/memkit.hpp>

static void test_strategy(memkit::hashmap_strategy strategy, const char* name)
{
    memkit::stl::array<std::byte, 16384> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::HashMap<std::uint32_t, std::int32_t> map;
    assert(memkit::ok(map.init_from_arena(arena, 4u, strategy, memkit::hashmap_policy::growable)));

    for (std::uint32_t i = 0u; i < 32u; ++i) {
        assert(memkit::ok(map.put(i, static_cast<std::int32_t>(i * 10))));
    }

    assert(map.size() == 32u);
    assert(map.bucket_count() >= 4u);

    for (std::uint32_t i = 0u; i < 32u; ++i) {
        assert(map.contains(i));
        std::int32_t out = 0;
        assert(memkit::ok(map.get(i, out)));
        assert(out == static_cast<std::int32_t>(i * 10));
    }

    assert(!map.contains(999u));
    std::int32_t missing = 0;
    assert(map.get(999u, missing) == memkit::status::not_found);

    for (std::uint32_t i = 0u; i < 16u; ++i) {
        assert(memkit::ok(map.remove(i)));
    }

    assert(map.size() == 16u);
    assert(!map.contains(0u));
    assert(memkit::ok(map.put(16u, -1)));

    std::int32_t got = 0;
    assert(memkit::ok(map.get(16u, got)));
    assert(got == -1);

    std::size_t visited = 0u;
    assert(memkit::ok(map.foreach([&visited](std::uint32_t, std::int32_t) {
        ++visited;
        return memkit::status::ok;
    })));
    assert(visited == 16u);

    std::printf("hashmap %s: ok\n", name);
}

static void test_caller_owned_open_addressing()
{
    enum { bucket_count = 8u };
    memkit::HashMap<std::uint32_t, std::int32_t>::open_slot slots[bucket_count]{};

    memkit::HashMap<std::uint32_t, std::int32_t> map;
    assert(memkit::ok(map.init(slots, bucket_count)));

    for (std::uint32_t i = 0u; i < bucket_count; ++i) {
        assert(memkit::ok(map.put(i, static_cast<std::int32_t>(i))));
    }

    assert(map.size() == bucket_count);
    assert(map.put(99u, 99) == memkit::status::full);
}

static void test_caller_owned_chaining()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::HashMap<std::uint32_t, std::int32_t> map;
    assert(memkit::ok(map.init_from_arena(arena, 8u, memkit::hashmap_strategy::chaining)));

    for (std::uint32_t i = 0u; i < 10u; ++i) {
        assert(memkit::ok(map.put(i, static_cast<std::int32_t>(i))));
    }

    assert(map.size() == 10u);
}

int main()
{
    test_strategy(memkit::hashmap_strategy::chaining, "chaining");
    test_strategy(memkit::hashmap_strategy::open_addressing, "open addressing");
    test_caller_owned_open_addressing();
    test_caller_owned_chaining();
    return 0;
}
