#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    const std::byte block_a[] = {std::byte{1}, std::byte{2}};
    const std::byte block_b[] = {std::byte{3}, std::byte{4}, std::byte{5}};
    std::byte block_c[]       = {std::byte{6}};

    memkit::FixedIoVec<4> iov;
    assert(iov.empty());
    assert(memkit::ok(iov.push(block_a, 2u)));
    assert(memkit::ok(iov.push(block_b, 3u)));
    assert(memkit::ok(iov.push(memkit::MutableIoSlice{block_c, 1u})));
    assert(iov.slice_count() == 3u);
    assert(iov.total_bytes() == 6u);
    assert(iov[1].size == 3u);
    assert(iov[1].data[2] == std::byte{5});

    const memkit::stl::span<const memkit::IoSlice> slices = iov.slices();
    assert(slices.size() == 3u);

    iov.clear();
    assert(iov.empty());

    std::printf("fixed_iovec: ok\n");
    return 0;
}
