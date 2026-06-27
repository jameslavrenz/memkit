#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/memkit.hpp>

struct item {
    std::uint32_t id;
    std::int32_t  value;
};

static void test_stack_caller_owned()
{
    memkit::stl::array<item, 4> storage{};

    memkit::Stack<item> stack;
    assert(memkit::ok(stack.init(storage)));

    for (std::uint32_t i = 0u; i < 4u; ++i) {
        assert(memkit::ok(stack.push(item{i, static_cast<std::int32_t>(i)})));
    }

    assert(stack.full());
    assert(stack.push(item{99u, 99}) == memkit::status::full);

    item top{};
    assert(memkit::ok(stack.peek(top)));
    assert(top.id == 3u);

    assert(memkit::ok(stack.pop(&top)));
    assert(top.id == 3u);
    assert(stack.size() == 3u);
}

static void test_stack_arena()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::Stack<item> stack;
    assert(memkit::ok(stack.init_from_arena(arena, 2u)));

    for (std::uint32_t i = 0u; i < 10u; ++i) {
        assert(memkit::ok(stack.push(item{i, static_cast<std::int32_t>(i)})));
    }

    assert(stack.size() == 10u);
}

static void test_queue_caller_owned()
{
    memkit::stl::array<item, 4> storage{};

    memkit::Queue<item> queue;
    assert(memkit::ok(queue.init(storage)));

    for (std::uint32_t i = 0u; i < 4u; ++i) {
        assert(memkit::ok(queue.push_back(item{i, static_cast<std::int32_t>(i * 10)})));
    }

    assert(queue.full());
    assert(queue.push_back(item{99u, 99}) == memkit::status::full);

    item front{};
    assert(memkit::ok(queue.peek_front(front)));
    assert(front.id == 0u);

    assert(memkit::ok(queue.pop_front(&front)));
    assert(front.id == 0u);
    assert(queue.size() == 3u);
}

static void test_queue_arena()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::Queue<item> queue;
    assert(memkit::ok(queue.init_from_arena(arena, 2u)));

    for (std::uint32_t i = 0u; i < 10u; ++i) {
        assert(memkit::ok(queue.push_back(item{i, static_cast<std::int32_t>(i)})));
    }

    assert(queue.size() == 10u);

    for (std::uint32_t i = 0u; i < 10u; ++i) {
        item out{};
        assert(memkit::ok(queue.pop_front(&out)));
        assert(out.id == i);
    }
}

int main()
{
    test_stack_caller_owned();
    test_stack_arena();
    test_queue_caller_owned();
    test_queue_arena();
    std::puts("stack/queue: ok");
    return 0;
}
