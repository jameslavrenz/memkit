#include <cassert>
#include <cstdint>

#include <memkit/config.hpp>
#include <memkit/memkit.hpp>

struct packet {
    std::uint32_t id;
    std::uint8_t  payload[8];
};

static void test_caller_owned_fixed()
{
    memkit::stl::array<packet, 4> storage{};

    memkit::Vector<packet> vector;
    assert(memkit::ok(vector.init(storage)));

    for (std::uint32_t i = 0u; i < 4u; ++i) {
        assert(memkit::ok(vector.push_back(packet{.id = i})));
    }

    assert(vector.size() == 4u);
    assert(vector.push_back(packet{.id = 99u}) == memkit::status::full);

    const packet* const back = vector.at(vector.size() - 1u);
    assert(back != nullptr);
    assert(back->id == 3u);

    vector.clear();
    assert(vector.empty());
}

static void test_arena_growable_vector()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::Vector<packet> vector;
    assert(memkit::ok(vector.init_from_arena(arena, 2u, memkit::vector_policy::growable)));

    for (std::uint32_t i = 0u; i < 10u; ++i) {
        assert(memkit::ok(vector.push_back(packet{.id = i})));
    }

    assert(vector.size() == 10u);
    assert(vector.capacity() >= 10u);

    const packet* const front = vector.at(0u);
    assert(front != nullptr);
    assert(front->id == 0u);
}

static void test_arena_owned_vector()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::Vector<packet> vector;
    assert(memkit::ok(vector.init_from_arena(arena, 2u)));

    std::size_t last_capacity = vector.capacity();
    for (std::uint32_t i = 0u; i < 20u; ++i) {
        assert(memkit::ok(vector.push_back(packet{.id = i})));
        if (vector.capacity() > last_capacity) {
            assert(vector.capacity() == last_capacity * 2u);
            last_capacity = vector.capacity();
        }
    }

    assert(vector.size() == 20u);
}

int main()
{
    test_caller_owned_fixed();
    test_arena_growable_vector();
    test_arena_owned_vector();
    return 0;
}
