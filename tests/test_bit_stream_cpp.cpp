#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    const std::byte payload[] = {std::byte{0xA5}, std::byte{0x3C}, std::byte{0xF0}};

    memkit::BitReader reader;
    assert(memkit::ok(reader.init(payload)));
    std::uint32_t byte0 = 0u;
    assert(memkit::ok(reader.read_bits(8u, byte0)));
    assert(byte0 == 0xA5u);

    std::uint32_t nibble = 0u;
    assert(memkit::ok(reader.read_bits(4u, nibble)));
    assert(nibble == 0x3u);

    bool bit = false;
    assert(memkit::ok(reader.read_bit(bit)));
    assert(bit);

    std::byte out[2]{};
    memkit::BitWriter writer;
    assert(memkit::ok(writer.init(out)));
    assert(memkit::ok(writer.write_bits(0xA5u, 8u)));
    assert(memkit::ok(writer.write_bits(0x3u, 4u)));
    assert(memkit::ok(writer.write_bit(true)));
    assert(writer.byte_length() == 2u);
    assert(out[0] == std::byte{0xA5});
    assert(out[1] == std::byte{0x38});

    std::printf("bit_stream: ok\n");
    return 0;
}
