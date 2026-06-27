#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    memkit::stl::array<std::byte, memkit::SpscQueue<int>::storage_bytes<4>()> backing{};
    memkit::SpscQueue<int> queue;
    assert(memkit::ok(queue.init(backing.data(), 4u)));

    assert(queue.empty());
    assert(queue.capacity() == 4u);

    assert(memkit::ok(queue.push(1)));
    assert(memkit::ok(queue.push(2)));
    assert(memkit::ok(queue.push(3)));
    assert(memkit::ok(queue.push(4)));
    assert(queue.full());
    assert(queue.size() == 4u);

    int value = 0;
    assert(memkit::ok(queue.peek(value)));
    assert(value == 1);
    assert(queue.size() == 4u);

    assert(memkit::ok(queue.pop(value)));
    assert(value == 1);
    assert(memkit::ok(queue.pop(value)));
    assert(value == 2);
    assert(memkit::ok(queue.pop(value)));
    assert(value == 3);
    assert(memkit::ok(queue.pop(value)));
    assert(value == 4);
    assert(queue.empty());

    memkit::stl::array<std::byte, 512> arena_backing{};
    memkit::memory::fixed_buffer buf{arena_backing};
    memkit::memory::static_arena arena{buf};

    memkit::SpscQueue<int> overwrite;
    assert(memkit::ok(overwrite.init_from_arena(
        arena, 4u, memkit::spsc_queue_policy::overwrite_on_full)));

    for (int i = 0; i < 6; ++i) {
        assert(memkit::ok(overwrite.push(i)));
    }

    assert(overwrite.size() == 4u);
    assert(memkit::ok(overwrite.pop(value)));
    assert(value == 2);

    std::printf("spsc_queue: ok\n");
    return 0;
}
