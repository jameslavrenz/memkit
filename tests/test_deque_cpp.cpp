#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/memkit.hpp>

struct item {
    std::uint32_t id;
    std::int32_t  value;
};

static void test_deque_caller_owned()
{
    memkit::stl::array<item, 4> storage{};

    memkit::Deque<item> deque;
    assert(memkit::ok(deque.init(storage)));

    const item back{1u, 10};
    const item front{2u, 20};
    assert(memkit::ok(deque.push_back(back)));
    assert(memkit::ok(deque.push_front(front)));
    assert(deque.size() == 2u);

    item peek{};
    assert(memkit::ok(deque.peek_front(peek)));
    assert(peek.id == 2u);
    assert(memkit::ok(deque.peek_back(peek)));
    assert(peek.id == 1u);

    for (std::uint32_t i = 0u; i < 2u; ++i) {
        assert(memkit::ok(deque.push_back(item{10u + i, static_cast<std::int32_t>(i)})));
    }

    assert(deque.full());
    assert(deque.push_front(item{99u, 99}) == memkit::status::full);

    assert(memkit::ok(deque.pop_front(&peek)));
    assert(peek.id == 2u);
    assert(memkit::ok(deque.pop_back(&peek)));
    assert(peek.id == 11u);
    assert(deque.size() == 2u);

    deque.clear();
    assert(deque.empty());
}

static void test_deque_arena()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::Deque<item> deque;
    assert(memkit::ok(deque.init_from_arena(arena, 2u)));

    for (std::uint32_t i = 0u; i < 10u; ++i) {
        const item value{i, static_cast<std::int32_t>(i)};
        if ((i % 2u) == 0u) {
            assert(memkit::ok(deque.push_back(value)));
        } else {
            assert(memkit::ok(deque.push_front(value)));
        }
    }

    assert(deque.size() == 10u);

    item front{};
    item back{};
    assert(memkit::ok(deque.peek_front(front)));
    assert(memkit::ok(deque.peek_back(back)));
    assert(front.id != back.id);
}

int main()
{
    test_deque_caller_owned();
    test_deque_arena();
    std::puts("deque: ok");
    return 0;
}
