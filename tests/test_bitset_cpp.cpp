#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/memkit.hpp>

static void test_bitset_caller_owned()
{
    enum { pin_count = 10u };
    memkit::stl::array<std::byte, memkit::Bitset::storage_bytes(pin_count)> storage{};

    memkit::Bitset pins;
    assert(memkit::ok(pins.init(storage.data(), pin_count)));
    assert(pins.empty());
    assert(pins.capacity() == pin_count);

    assert(memkit::ok(pins.set(1u)));
    assert(memkit::ok(pins.set(3u)));
    assert(memkit::ok(pins.set(7u)));
    assert(pins.size() == 3u);
    assert(pins.test(3u));
    assert(!pins.test(4u));

    assert(memkit::ok(pins.toggle(3u)));
    assert(!pins.test(3u));

    assert(memkit::ok(pins.set_all()));
    assert(pins.full());
    assert(pins.size() == pin_count);

    pins.clear();
    assert(pins.empty());

    std::size_t pin = 0u;
    assert(memkit::ok(pins.find_first_clear(0u, pin)));
    assert(pin == 0u);
    assert(memkit::ok(pins.set(pin)));
}

static void test_bitset_logical_ops()
{
    enum { capacity = 8u };
    memkit::stl::array<std::byte, 1> left_storage{};
    memkit::stl::array<std::byte, 1> right_storage{};
    memkit::stl::array<std::byte, 1> tmp_storage{};

    memkit::Bitset left;
    memkit::Bitset right;
    memkit::Bitset tmp;

    assert(memkit::ok(left.init(left_storage.data(), capacity)));
    assert(memkit::ok(right.init(right_storage.data(), capacity)));
    assert(memkit::ok(tmp.init(tmp_storage.data(), capacity)));

    assert(memkit::ok(left.set(1u)));
    assert(memkit::ok(left.set(2u)));
    assert(memkit::ok(left.set(5u)));
    assert(memkit::ok(right.set(2u)));
    assert(memkit::ok(right.set(3u)));
    assert(memkit::ok(right.set(5u)));

    assert(memkit::ok(tmp.copy_from(left)));
    assert(memkit::ok(tmp.intersect_with(right)));
    assert(tmp.test(2u));
    assert(tmp.test(5u));
    assert(!tmp.test(1u));
    assert(tmp.size() == 2u);
}

static void test_bitset_arena()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::Bitset mask;
    assert(memkit::ok(mask.init_from_arena(arena, 32u)));

    for (std::size_t i = 0u; i < 32u; i += 3u) {
        assert(memkit::ok(mask.set(i)));
    }

    std::size_t found = 0u;
    assert(memkit::ok(mask.find_first_set(0u, found)));
    assert(found == 0u);
    assert(memkit::ok(mask.find_first_set(1u, found)));
    assert(found == 3u);
}

int main()
{
    test_bitset_caller_owned();
    test_bitset_logical_ops();
    test_bitset_arena();
    std::puts("bitset: ok");
    return 0;
}
