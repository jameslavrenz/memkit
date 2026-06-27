#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/memkit.hpp>

struct block {
    std::uint32_t id;
    std::int32_t  value;
};

static void test_objpool_caller_owned()
{
    enum { capacity = 4u };

    memkit::stl::array<std::byte, 256> storage{};
    memkit::stl::array<std::uint32_t, capacity> free_stack{};
    memkit::stl::array<std::byte, 8> used_bits{};

    memkit::ObjPool<block> pool;
    assert(memkit::ok(pool.init(
        storage.data(),
        free_stack.data(),
        used_bits.data(),
        capacity
    )));

    assert(pool.empty());
    assert(pool.capacity() == capacity);
    assert(pool.available() == capacity);

    block* slots[capacity] = {};
    for (std::uint32_t i = 0u; i < capacity; ++i) {
        assert(memkit::ok(pool.alloc(&slots[i])));
        slots[i]->id = i;
        slots[i]->value = static_cast<std::int32_t>(i * 10);
    }

    assert(pool.full());
    block* extra = nullptr;
    assert(pool.alloc(&extra) == memkit::status::full);

    for (std::uint32_t i = 0u; i < capacity; ++i) {
        assert(pool.contains(slots[i]));
    }

    assert(memkit::ok(pool.free(slots[2])));
    assert(pool.available() == 1u);

    block source{.id = 42u, .value = 420};
    block* reused = nullptr;
    assert(memkit::ok(pool.alloc_copy(source, &reused)));
    assert(reused == slots[2]);
    assert(reused->id == 42u);

    pool.clear();
    assert(pool.empty());
    assert(pool.available() == capacity);
}

static void test_objpool_arena()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::ObjPool<block> pool;
    assert(memkit::ok(pool.init_from_arena(arena, 8u)));

    block* first = nullptr;
    block* second = nullptr;
    assert(memkit::ok(pool.alloc(&first)));
    assert(memkit::ok(pool.alloc(&second)));

    first->id = 1u;
    second->id = 2u;

    std::size_t index = 0u;
    assert(memkit::ok(pool.index(second, index)));
    assert(index == 1u);

    assert(memkit::ok(pool.free(first)));
    block* third = nullptr;
    assert(memkit::ok(pool.alloc(&third)));
    assert(third == first);
}

int main()
{
    test_objpool_caller_owned();
    test_objpool_arena();
    std::puts("objpool: ok");
    return 0;
}
