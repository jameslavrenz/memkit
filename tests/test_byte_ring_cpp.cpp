#include <cassert>
#include <cstdio>
#include <cstring>

#include <memkit/memkit.hpp>

int main()
{
    memkit::stl::array<std::byte, 16> backing{};
    memkit::ByteRing ring;
    assert(memkit::ok(ring.init(backing, 8u)));

    const char payload[] = "hello";
    std::size_t written = 0u;
    assert(memkit::ok(ring.push_bytes(
        reinterpret_cast<const std::uint8_t*>(payload),
        sizeof payload - 1u,
        &written
    )));
    assert(written == 5u);
    assert(ring.size() == 5u);

    const std::uint8_t* rx = nullptr;
    const std::size_t rx_n = ring.readable_contiguous(&rx);
    assert(rx_n >= 5u);
    assert(std::memcmp(rx, "hello", 5) == 0);
    ring.commit_read(5u);
    assert(ring.empty());

    memkit::stl::array<std::byte, 512> arena_backing{};
    memkit::memory::fixed_buffer buf{arena_backing};
    memkit::memory::static_arena arena{buf};

    memkit::ByteRing log;
    assert(memkit::ok(log.init_from_arena(
        arena, 4u, memkit::byte_ring_policy::overwrite_on_full)));

    for (std::uint8_t i = 0u; i < 6u; ++i) {
        assert(memkit::ok(log.push_byte(static_cast<std::uint8_t>('0' + i))));
    }

    assert(log.size() == 4u);
    std::uint8_t out = 0u;
    assert(memkit::ok(log.pop_byte(&out)));
    assert(out == '2');

    std::printf("byte_ring: ok\n");
    return 0;
}
