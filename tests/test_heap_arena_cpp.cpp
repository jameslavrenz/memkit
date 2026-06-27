#include <cassert>
#include <cstdint>

#include <memkit/config.hpp>
#include <memkit/memkit.hpp>

int main()
{
#if MEMKIT_ALLOW_HEAP
    auto backing = memkit::memory::heap_storage::allocate(4096u);
    assert(backing.valid());

    memkit::memory::heap_arena arena{std::move(backing)};
    assert(arena.capacity_bytes() == 4096u);
    assert(arena.used_bytes() == 0u);

    void* block = nullptr;
    assert(memkit::ok(arena.allocate(128u, 8u, &block)));
    assert(block != nullptr);
    assert(arena.used_bytes() >= 128u);

    arena.reset();
    assert(arena.used_bytes() == 0u);

    memkit::Ring<std::uint32_t> ring;
    assert(memkit::ok(ring.init_from_arena(
        arena, 16u, memkit::ring_policy::overwrite_on_full)));

    for (std::uint32_t i = 0u; i < 20u; ++i) {
        assert(memkit::ok(ring.push_back(i)));
    }

    assert(ring.size() == 16u);
    assert(ring.capacity() == 16u);

    std::uint32_t front = 0u;
    assert(memkit::ok(ring.peek_front(front)));
    assert(front == 4u);
#else
    assert(false && "test_heap_arena_cpp requires MEMKIT_ALLOW_HEAP");
#endif
    return 0;
}
