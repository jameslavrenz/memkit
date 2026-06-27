/*
 * MCU-style usage: std::array backing + typed ring buffer (C++26 API).
 */

#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/memkit.hpp>

enum { SENSOR_QUEUE_CAPACITY = 16 };

struct sensor_sample {
    std::uint32_t timestamp_ms;
    std::int16_t  value;
};

int main()
{
    memkit::stl::array<std::byte, 512> arena_backing{};
    memkit::stl::array<sensor_sample, SENSOR_QUEUE_CAPACITY> ring_storage{};

    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::Ring<sensor_sample> queue;
    assert(memkit::ok(queue.init(ring_storage)));

    memkit::Ring<sensor_sample> log_ring;
    assert(memkit::ok(log_ring.init_from_arena(
        arena, 8u, memkit::ring_policy::overwrite_on_full)));

    for (std::uint32_t i = 0u; i < SENSOR_QUEUE_CAPACITY; ++i) {
        const sensor_sample sample{i * 10u, static_cast<std::int16_t>(i * 3)};
        assert(memkit::ok(queue.push_back(sample)));
        assert(memkit::ok(log_ring.push_back(sample)));
    }

    for (std::uint32_t i = SENSOR_QUEUE_CAPACITY; i < 20u; ++i) {
        const sensor_sample sample{i * 10u, static_cast<std::int16_t>(i * 3)};
        assert(memkit::ok(log_ring.push_back(sample)));
    }

    std::printf("queue size=%zu capacity=%zu\n", queue.size(), queue.capacity());
    std::printf("log_ring size=%zu capacity=%zu\n", log_ring.size(), log_ring.capacity());

    const memkit::stl::optional<sensor_sample> oldest = queue.try_peek_front();
    assert(oldest.has_value());
    std::printf(
        "oldest queue sample: ts=%u value=%d\n",
        oldest->timestamp_ms,
        oldest->value
    );

    const sensor_sample* rx_ptr = nullptr;
    const std::size_t rx_elems = queue.readable_contiguous(&rx_ptr);
    std::printf("contiguous readable elements=%zu ptr=%p\n", rx_elems, static_cast<const void*>(rx_ptr));
    queue.commit_read(rx_elems);

    return 0;
}
