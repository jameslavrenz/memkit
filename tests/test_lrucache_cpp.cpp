#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/config.hpp>
#include <memkit/memkit.hpp>

static void test_lrucache_caller_owned()
{
    enum { capacity = 3u, buckets = 4u };

    memkit::stl::array<std::byte, 256> pool{};
    memkit::detail::lrucache_entry_header* bucket_heads[buckets]{};

    memkit::LruCache<std::uint32_t, std::int32_t> cache;
    assert(memkit::ok(cache.init(pool.data(), capacity, bucket_heads, buckets)));

    for (std::uint32_t key = 1u; key <= 3u; ++key) {
        assert(memkit::ok(cache.put(key, static_cast<std::int32_t>(key * 10))));
    }

    assert(cache.full());

    std::int32_t value = 0;
    assert(memkit::ok(cache.get(1u, value)));
    assert(value == 10);

    assert(memkit::ok(cache.put(4u, 40)));
    assert(!cache.contains(2u));
    assert(cache.contains(1u));
    assert(cache.contains(3u));
    assert(cache.contains(4u));

    std::uint32_t order[3] = {};
    std::size_t count = 0u;
    assert(memkit::ok(cache.foreach_mru([&](std::uint32_t key, std::int32_t) {
        order[count++] = key;
        return memkit::status::ok;
    })));
    assert(count == 3u);
    assert(order[0] == 4u);
    assert(order[1] == 1u);
    assert(order[2] == 3u);
}

static void test_lrucache_peek_no_promote()
{
    enum { capacity = 3u, buckets = 4u };
    memkit::stl::array<std::byte, 256> pool{};
    memkit::detail::lrucache_entry_header* bucket_heads[buckets]{};

    memkit::LruCache<std::uint32_t, std::int32_t> cache;
    assert(memkit::ok(cache.init(pool.data(), capacity, bucket_heads, buckets)));

    for (std::uint32_t key = 1u; key <= 3u; ++key) {
        assert(memkit::ok(cache.put(key, static_cast<std::int32_t>(key))));
    }

    std::int32_t out = 0;
    assert(memkit::ok(cache.peek(3u, out)));
    assert(out == 3);
    assert(memkit::ok(cache.put(4u, 4)));
    assert(!cache.contains(1u));
}

static void test_lrucache_arena()
{
    memkit::stl::array<std::byte, 8192> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::LruCache<std::uint32_t, std::int32_t> cache;
    assert(memkit::ok(cache.init_from_arena(arena, 8u)));

    for (std::uint32_t i = 0u; i < 8u; ++i) {
        assert(memkit::ok(cache.put(i, static_cast<std::int32_t>(i))));
    }

    assert(cache.full());
    assert(memkit::ok(cache.touch(0u)));
    assert(memkit::ok(cache.put(99u, 990)));
    assert(cache.contains(0u));
    assert(cache.contains(99u));
}

int main()
{
    test_lrucache_caller_owned();
    test_lrucache_peek_no_promote();
    test_lrucache_arena();
    std::puts("lrucache: ok");
    return 0;
}
