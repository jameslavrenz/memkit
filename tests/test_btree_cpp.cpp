#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/config.hpp>
#include <memkit/memkit.hpp>

static void test_caller_owned_node_pool()
{
    enum { capacity = 8u };
    memkit::stl::array<std::byte, 512> pool{};

    memkit::BTree<std::uint32_t, std::int32_t> tree;
    assert(memkit::ok(tree.init(pool.data(), capacity)));

    const std::uint32_t keys[] = {5u, 2u, 8u, 1u, 9u, 3u, 7u, 4u};
    for (std::uint32_t key : keys) {
        assert(memkit::ok(tree.insert(key, static_cast<std::int32_t>(key))));
    }

    assert(tree.full());
    assert(tree.insert(99u, 99) == memkit::status::full);

    std::uint32_t sorted[8] = {};
    std::size_t index = 0u;
    assert(memkit::ok(tree.foreach(memkit::btree_traversal::inorder, [&sorted, &index](std::uint32_t key, std::int32_t) {
        sorted[index++] = key;
        return memkit::status::ok;
    })));

    const std::uint32_t expected[] = {1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u};
    for (std::size_t i = 0u; i < 8u; ++i) {
        assert(sorted[i] == expected[i]);
    }

    std::int32_t found = 0;
    assert(memkit::ok(tree.get(7u, found)));
    assert(found == 7);
    assert(memkit::ok(tree.remove(7u)));
    assert(tree.size() == 7u);
    assert(!tree.contains(7u));
}

static void test_arena_dynamic_nodes()
{
    memkit::stl::array<std::byte, 8192> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::BTree<std::uint32_t, std::int32_t> tree;
    assert(memkit::ok(tree.init_from_arena(arena, 0u, memkit::btree_policy::none)));

    for (std::uint32_t i = 0u; i < 20u; ++i) {
        assert(memkit::ok(tree.insert(i, static_cast<std::int32_t>(i * 2))));
    }

    assert(tree.size() == 20u);

    std::uint32_t min_key = 0u;
    std::uint32_t max_key = 0u;
    std::int32_t min_val = 0;
    std::int32_t max_val = 0;
    assert(memkit::ok(tree.peek_min(min_key, min_val)));
    assert(memkit::ok(tree.peek_max(max_key, max_val)));
    assert(min_key == 0u);
    assert(max_key == 19u);

    assert(memkit::ok(tree.insert(10u, -1)));
    std::int32_t got = 0;
    assert(memkit::ok(tree.get(10u, got)));
    assert(got == -1);

    for (std::uint32_t i = 0u; i < 20u; ++i) {
        assert(memkit::ok(tree.remove(i)));
    }

    assert(tree.empty());
}

static void test_btree_create()
{
    memkit::stl::array<std::byte, 4096> arena_backing{};
    memkit::memory::fixed_buffer backing{arena_backing};
    memkit::memory::static_arena arena{backing};

    memkit::BTree<std::uint32_t, std::int32_t> tree;
    assert(memkit::ok(tree.init_from_arena(arena, 0u, memkit::btree_policy::none)));

    assert(memkit::ok(tree.insert(42u, 100)));
    assert(tree.contains(42u));
}

int main()
{
    test_caller_owned_node_pool();
    test_arena_dynamic_nodes();
    test_btree_create();
    std::puts("btree: ok");
    return 0;
}
