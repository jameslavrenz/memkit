#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

struct log_record {
    std::uint32_t tick = 0u;
    std::int16_t  value = 0;
};

int main()
{
    memkit::stl::array<std::byte, memkit::RingLog<log_record>::storage_bytes<4>()> backing{};
    memkit::RingLog<log_record> log;
    assert(memkit::ok(log.init(backing.data(), 4u)));

    for (std::uint32_t i = 0u; i < 6u; ++i) {
        assert(memkit::ok(log.append(log_record{i, static_cast<std::int16_t>(i * 10)})));
    }

    assert(log.full());
    assert(log.size() == 4u);

    log_record oldest{};
    assert(memkit::ok(log.peek_oldest(oldest)));
    assert(oldest.tick == 2u);
    assert(oldest.value == 20);

    log_record newest{};
    assert(memkit::ok(log.peek_newest(newest)));
    assert(newest.tick == 5u);
    assert(newest.value == 50);

    std::uint32_t chronological[4]{};
    std::size_t chrono_count = 0u;
    assert(memkit::ok(log.foreach_chronological([&](const log_record& rec) {
        chronological[chrono_count++] = rec.tick;
        return memkit::status::ok;
    })));
    assert(chrono_count == 4u);
    assert(chronological[0] == 2u);
    assert(chronological[3] == 5u);

    std::uint32_t reverse[4]{};
    std::size_t reverse_count = 0u;
    assert(memkit::ok(log.foreach_newest_first([&](const log_record& rec) {
        reverse[reverse_count++] = rec.tick;
        return memkit::status::ok;
    })));
    assert(reverse_count == 4u);
    assert(reverse[0] == 5u);
    assert(reverse[3] == 2u);

    std::printf("ring_log: ok\n");
    return 0;
}
