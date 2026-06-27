#include <cassert>
#include <cstdio>
#include <cstring>

#include <memkit/memkit.hpp>

int main()
{
    memkit::SmallBuffer<8> buf;
    assert(buf.empty());

    const std::byte payload[] = {
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}
    };
    assert(memkit::ok(buf.assign(payload, 3u)));
    assert(buf.size() == 3u);
    assert(buf.view()[0] == std::byte{0x01});

    assert(memkit::ok(buf.append(std::byte{0xFF})));
    assert(buf.size() == 4u);

    memkit::SmallBuffer<2> tiny;
    assert(tiny.assign(payload, 3u) == memkit::status::invalid);
    assert(memkit::ok(tiny.assign(payload, 2u)));

    assert(memkit::ok(buf.resize(6u, std::byte{0xAA})));
    assert(buf.size() == 6u);
    assert(buf.data()[5] == std::byte{0xAA});

    memkit::SmallBuffer<8> copy;
    assert(memkit::ok(copy.assign(buf.view())));
    assert(copy == buf);

    std::printf("small_buffer: ok\n");
    return 0;
}
