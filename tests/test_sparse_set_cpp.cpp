#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    memkit::stl::array<std::size_t, 8> dense{};
    memkit::stl::array<std::size_t, 8> sparse{};
    memkit::SparseSet set;
    assert(memkit::ok(set.init(dense, sparse)));
    assert(set.max_id() == 8u);

    assert(memkit::ok(set.insert(1u)));
    assert(memkit::ok(set.insert(5u)));
    assert(memkit::ok(set.insert(3u)));
    assert(set.size() == 3u);
    assert(set.contains(5u));
    assert(!set.contains(2u));

    assert(memkit::ok(set.remove(5u)));
    assert(!set.contains(5u));
    assert(set.size() == 2u);

    std::size_t sum = 0u;
    for (std::size_t i = 0u; i < set.size(); ++i) {
        sum += set[i];
    }
    assert(sum == 4u);

    memkit::stl::array<std::byte, 512> arena_backing{};
    memkit::memory::fixed_buffer buf{arena_backing};
    memkit::memory::static_arena arena{buf};

    memkit::SparseSet arena_set;
    assert(memkit::ok(arena_set.init_from_arena(arena, 16u)));
    assert(memkit::ok(arena_set.insert(7u)));
    assert(arena_set.contains(7u));

    std::printf("sparse_set: ok\n");
    return 0;
}
