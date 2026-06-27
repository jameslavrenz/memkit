#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/memkit.hpp>

struct item {
    std::uint32_t id;
    std::int32_t  value;
};

static void test_caller_owned_node_pool()
{
    enum { capacity = 4u };
    memkit::stl::array<std::byte, 256> pool{};

    memkit::List<item> list;
    assert(memkit::ok(list.init(pool.data(), capacity)));
    assert(list.empty());
    assert(list.capacity() == capacity);

    for (std::uint32_t i = 0u; i < capacity; ++i) {
        assert(memkit::ok(list.push_back(item{i, static_cast<std::int32_t>(i * 10)})));
    }

    assert(list.full());
    assert(list.push_back(item{99u, 99}) == memkit::status::full);

    item front{};
    assert(memkit::ok(list.peek_front(front)));
    assert(front.id == 0u);

    assert(memkit::ok(list.pop_front()));
    assert(list.size() == 3u);
    list.clear();
    assert(list.empty());
}

static void test_arena_dynamic_nodes()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::List<item> list;
    assert(memkit::ok(list.init_from_arena(arena, 16u)));

    for (std::uint32_t i = 0u; i < 10u; ++i) {
        assert(memkit::ok(list.push_front(item{i, static_cast<std::int32_t>(i)})));
    }

    assert(list.size() == 10u);

    item back{};
    assert(memkit::ok(list.peek_back(back)));
    assert(back.id == 0u);

    assert(memkit::ok(list.insert_at(5u, item{50u, 500})));
    item at{};
    assert(memkit::ok(list.peek_at(5u, at)));
    assert(at.id == 50u);

    std::int32_t sum = 0;
    assert(memkit::ok(list.for_each([&](const item& elem, std::size_t) {
        sum += elem.value;
        return memkit::status::ok;
    })));

    const std::uint32_t remove_id = 50u;
    assert(memkit::ok(list.remove_first([&](const item& elem) {
        return elem.id == remove_id;
    })));
    assert(list.size() == 10u);
}

static void test_list_create()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::List<item> list;
    assert(memkit::ok(list.init_from_arena(arena, 16u)));
    assert(memkit::ok(list.push_back(item{1u, 42})));
    assert(list.size() == 1u);
}

int main()
{
    test_caller_owned_node_pool();
    test_arena_dynamic_nodes();
    test_list_create();
    std::puts("list: ok");
    return 0;
}
