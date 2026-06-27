#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    memkit::stl::array<std::byte, memkit::DoubleBuffer<int>::storage_bytes<4>()> backing{};
    memkit::DoubleBuffer<int> buffer;
    assert(memkit::ok(buffer.init(backing.data(), 4u)));
    assert(buffer.slot_capacity() == 4u);

    memkit::stl::span<int> write = buffer.write_span();
    for (int i = 0; i < 4; ++i) {
        write[static_cast<std::size_t>(i)] = i + 1;
    }
    buffer.publish();

    memkit::stl::span<const int> read = buffer.read_span();
    assert(read[0] == 1);
    assert(read[3] == 4);

    write = buffer.write_span();
    write[0] = 100;
    buffer.publish();
    read = buffer.read_span();
    assert(read[0] == 100);

    memkit::stl::array<std::byte, 512> arena_backing{};
    memkit::memory::fixed_buffer buf{arena_backing};
    memkit::memory::static_arena arena{buf};

    memkit::DoubleBuffer<std::uint16_t> dma;
    assert(memkit::ok(dma.init_from_arena(arena, 8u)));
    dma.write_span()[0] = 0xBEEFu;
    dma.publish();
    assert(dma.read_span()[0] == 0xBEEFu);

    std::printf("double_buffer: ok\n");
    return 0;
}
