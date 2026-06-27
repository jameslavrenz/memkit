#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/memkit.hpp>

struct task {
    std::uint32_t id;
    std::int32_t  priority;
};

struct task_compare {
    bool operator()(const task& left, const task& right) const noexcept
    {
        return left.priority < right.priority;
    }
};

static void test_pqueue_caller_owned()
{
    memkit::stl::array<task, 4> storage{};

    memkit::PQueue<task, task_compare> pqueue{task_compare{}};
    assert(memkit::ok(pqueue.init(
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size(),
        memkit::pqueue_policy::none,
        task_compare{}
    )));

    const task items[] = {
        {0u, 5},
        {1u, 1},
        {2u, 3},
        {3u, 2},
    };

    for (const task& item : items) {
        assert(memkit::ok(pqueue.push(item)));
    }

    assert(pqueue.full());
    assert(pqueue.push(task{99u, 0}) == memkit::status::full);

    task top{};
    assert(memkit::ok(pqueue.peek(top)));
    assert(top.priority == 1);

    const std::int32_t expected_order[] = {1, 2, 3, 5};
    for (std::int32_t expected : expected_order) {
        task out{};
        assert(memkit::ok(pqueue.pop(&out)));
        assert(out.priority == expected);
    }

    assert(pqueue.empty());
}

static void test_pqueue_arena()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::PQueue<task, task_compare> pqueue{task_compare{}};
    assert(memkit::ok(pqueue.init_from_arena(arena, 16u, memkit::pqueue_policy::none, task_compare{})));

    for (std::uint32_t i = 0u; i < 16u; ++i) {
        assert(memkit::ok(pqueue.push(task{i, static_cast<std::int32_t>(i % 5u)})));
    }

    assert(pqueue.size() == 16u);

    int previous = -1;
    while (!pqueue.empty()) {
        task out{};
        assert(memkit::ok(pqueue.pop(&out)));
        assert(out.priority >= previous);
        previous = out.priority;
    }
}

int main()
{
    test_pqueue_caller_owned();
    test_pqueue_arena();
    std::puts("pqueue: ok");
    return 0;
}
