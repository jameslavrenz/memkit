#include <cassert>
#include <cstdio>
#include <cstdint>

#include <memkit/memkit.hpp>

int main()
{
    enum { capacity = 4u };

    memkit::stl::array<std::byte, 64> storage{};
    memkit::stl::array<std::uint16_t, capacity> generations{};
    memkit::stl::array<std::uint32_t, capacity> free_stack{};

    memkit::HandlePool<std::uint32_t> pool;
    assert(memkit::ok(pool.init(
        storage.data(),
        generations.data(),
        free_stack.data(),
        capacity
    )));

    memkit::handle_t h0 = memkit::invalid_handle;
    memkit::handle_t h1 = memkit::invalid_handle;
    assert(memkit::ok(pool.emplace(h0, 100u)));
    assert(memkit::ok(pool.emplace(h1, 200u)));
    assert(pool.size() == 2u);
    assert(h0 != h1);
    assert(pool.valid(h0));
    assert(pool.valid(h1));

    std::uint32_t* p0 = nullptr;
    assert(memkit::ok(pool.get(h0, p0)));
    assert(*p0 == 100u);

    assert(memkit::ok(pool.release(h0)));
    assert(!pool.valid(h0));
    assert(pool.valid(h1));

    memkit::handle_t h2 = memkit::invalid_handle;
    assert(memkit::ok(pool.emplace(h2, 300u)));
    assert(h2 != h0);
    assert(pool.get(h0, p0) == memkit::status::invalid);

    memkit::stl::array<std::byte, 512> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::HandlePool<std::int32_t> arena_pool;
    assert(memkit::ok(arena_pool.init_from_arena(arena, 8u)));
    memkit::handle_t ah = memkit::invalid_handle;
    assert(memkit::ok(arena_pool.emplace(ah, -42)));
    std::int32_t* ap = nullptr;
    assert(memkit::ok(arena_pool.get(ah, ap)));
    assert(*ap == -42);

    std::printf("handle_pool: ok\n");
    return 0;
}
