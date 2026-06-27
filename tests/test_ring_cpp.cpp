#include <cassert>
#include <cstdint>

#include <memkit/memkit.hpp>

struct packet {
    std::uint32_t id;
    std::uint8_t payload[8];
};

static void test_caller_owned_static()
{
    memkit::stl::array<packet, 4> storage{};

    memkit::Ring<packet> ring;
    assert(memkit::ok(ring.init(storage)));
    assert(ring.empty());
    assert(ring.capacity() == 4u);

    for (std::uint32_t i = 0u; i < 4u; ++i) {
        assert(memkit::ok(ring.push_back(packet{.id = i})));
    }

    assert(ring.full());
    assert(ring.size() == 4u);

    const memkit::stl::optional<packet> popped = ring.try_pop_front();
    assert(popped.has_value());
    assert(popped->id == 0u);
    assert(ring.size() == 3u);

    ring.clear();
    assert(ring.empty());
}

static void test_arena_owned_ring()
{
    memkit::stl::array<std::byte, 1024> arena_backing{};

    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::Ring<packet> ring;
    assert(memkit::ok(ring.init_from_arena(
        arena, 8u, memkit::ring_policy::overwrite_on_full)));

    for (std::uint32_t i = 0u; i < 12u; ++i) {
        assert(memkit::ok(ring.push_back(packet{.id = i})));
    }

    assert(ring.size() == 8u);

    const packet* rx_ptr = nullptr;
    const std::size_t rx_elems = ring.readable_contiguous(&rx_ptr);
    assert(rx_elems > 0u);
    assert(rx_ptr != nullptr);

    const memkit::stl::optional<packet> newest = ring.try_peek_back();
    assert(newest.has_value());
    assert(newest->id == 11u);
}

int main()
{
    test_caller_owned_static();
    test_arena_owned_ring();
    return 0;
}
