#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <memkit/config.hpp>
#include <memkit/memory/memory.hpp>
#include <memkit/status.hpp>
#include <memkit/stl.hpp>

int main()
{
    memkit::stl::array<std::byte, 64> backing{};
    memkit::memory::static_arena arena{backing};

    void* block = nullptr;
    assert(memkit::ok(arena.allocate(32u, 8u, &block)));
    assert(block != nullptr);

    void* too_large = nullptr;
    assert(arena.allocate(64u, 8u, &too_large) == memkit::status::oom);
    assert(too_large == nullptr);

    arena.reset();

    memkit::stl::array<std::byte, 16> tight{};
    memkit::memory::static_arena tight_arena{tight};

    void* first = nullptr;
    assert(memkit::ok(tight_arena.allocate(9u, 8u, &first)));

    void* second = nullptr;
    assert(tight_arena.allocate(8u, 8u, &second) == memkit::status::oom);

    void* calloc_overflow = nullptr;
    assert(tight_arena.calloc(2u, SIZE_MAX, 8u, &calloc_overflow) == memkit::status::invalid);

    printf("arena_cpp: ok\n");
    return 0;
}
