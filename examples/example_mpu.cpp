/*
 * MPU-style usage: mmap-backed arena and arena-owned ring (C++26 API).
 * Build with: make mpu
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>

#include <memkit/config.hpp>
#include <memkit/memkit.hpp>

struct log_line {
    char message[64];
};

int main()
{
#if MEMKIT_ALLOW_MMAP
    auto backing = memkit::memory::mmap_storage::map(4096u);
    assert(backing.valid());

    memkit::memory::mmap_arena arena{std::move(backing)};

    memkit::Ring<log_line> logs;
    assert(memkit::ok(logs.init_from_arena(
        arena, 32u, memkit::ring_policy::overwrite_on_full)));

    for (int i = 0; i < 40; ++i) {
        log_line line{};
        std::snprintf(line.message, sizeof line.message, "event %d", i);
        assert(memkit::ok(logs.push_back(line)));
    }

    std::printf("log ring size=%zu (capacity=%zu)\n", logs.size(), logs.capacity());

    log_line front{};
    assert(memkit::ok(logs.peek_front(front)));
    std::printf("oldest retained log: %s\n", front.message);
#else
    std::puts("example_mpu: skipped (MEMKIT_ALLOW_MMAP=0)");
#endif
    return 0;
}
