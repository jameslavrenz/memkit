#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    alignas(std::max_align_t) memkit::stl::array<std::byte, memkit::MpscQueue<int>::storage_bytes<4>()> backing{};
    memkit::MpscQueue<int> queue;
    assert(memkit::ok(queue.init(backing.data(), 4u)));
    assert(queue.empty());

    assert(memkit::ok(queue.push(10)));
    assert(memkit::ok(queue.push(20)));
    assert(memkit::ok(queue.push(30)));
    assert(queue.size() == 3u);

    int value = 0;
    assert(memkit::ok(queue.pop(value)));
    assert(value == 10);
    assert(memkit::ok(queue.pop(value)));
    assert(value == 20);
    assert(memkit::ok(queue.pop(value)));
    assert(value == 30);
    assert(queue.empty());

    for (int i = 0; i < 4; ++i) {
        assert(memkit::ok(queue.push(i)));
    }
    assert(queue.full());
    assert(memkit::ok(queue.pop(value)));
    assert(value == 0);

    memkit::stl::array<std::byte, 512> arena_backing{};
    memkit::memory::fixed_buffer buf{arena_backing};
    memkit::memory::static_arena arena{buf};

    memkit::MpscQueue<int> shared;
    assert(memkit::ok(shared.init_from_arena(arena, 4u)));
    assert(memkit::ok(shared.push(99)));
    assert(memkit::ok(shared.pop(value)));
    assert(value == 99);

    std::printf("mpsc_queue: ok\n");
    return 0;
}
