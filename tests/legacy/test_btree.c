#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "btree.h"

typedef struct {
    uint32_t key;
    int32_t value;
} item_t;

static int item_compare(const void *a, const void *b, void *user)
{
    (void)user;
    const item_t *left = (const item_t *)a;
    const item_t *right = (const item_t *)b;

    if (left->key < right->key) {
        return -1;
    }
    if (left->key > right->key) {
        return 1;
    }
    return 0;
}

static btree_status_t collect_inorder(const void *elem, void *user)
{
    item_t **cursor = (item_t **)user;
    **cursor = *(const item_t *)elem;
    (*cursor)++;
    return BTREE_OK;
}

static void test_caller_owned_node_pool(void)
{
    enum { CAPACITY = 8u };
    const size_t stride = btree_node_stride(sizeof(item_t));
    static uint8_t pool[CAPACITY * 64u];

    btree_t tree;
    assert(btree_status_ok(btree_init(&tree, &(btree_config_t){
        .elem_size = sizeof(item_t),
        .node_capacity = CAPACITY,
        .node_pool = pool,
        .node_pool_bytes = CAPACITY * stride,
        .compare_fn = item_compare,
    })));

    const uint32_t keys[] = { 5u, 2u, 8u, 1u, 9u, 3u, 7u, 4u };
    for (size_t i = 0u; i < sizeof keys / sizeof keys[0]; ++i) {
        const item_t item = { .key = keys[i], .value = (int32_t)keys[i] };
        assert(btree_status_ok(btree_insert(&tree, &item)));
    }

    assert(btree_full(&tree));

    const item_t extra = { .key = 99u, .value = 99 };
    assert(btree_insert(&tree, &extra) == BTREE_ERR_FULL);

    item_t sorted[8];
    item_t *cursor = sorted;
    assert(btree_status_ok(btree_foreach(
        &tree,
        BTREE_TRAVERSAL_INORDER,
        collect_inorder,
        &cursor
    )));

    const uint32_t expected[] = { 1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u };
    for (size_t i = 0u; i < 8u; ++i) {
        assert(sorted[i].key == expected[i]);
    }

    const item_t lookup_key = { .key = 7u };
    item_t found = {0};
    assert(btree_status_ok(btree_get(&tree, &lookup_key, &found)));
    assert(found.value == 7);

    assert(btree_status_ok(btree_remove(&tree, &lookup_key)));
    assert(btree_size(&tree) == 7u);
    assert(!btree_contains(&tree, &lookup_key));

    btree_deinit(&tree);
}

static void test_arena_dynamic_nodes(void)
{
    static uint8_t arena_backing[8192];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    btree_t tree;
    assert(btree_status_ok(btree_init(&tree, &(btree_config_t){
        .elem_size = sizeof(item_t),
        .arena = &arena,
        .compare_fn = item_compare,
    })));

    for (uint32_t i = 0u; i < 20u; ++i) {
        const item_t item = { .key = i, .value = (int32_t)(i * 2) };
        assert(btree_status_ok(btree_insert(&tree, &item)));
    }

    assert(btree_size(&tree) == 20u);

    item_t min = {0};
    item_t max = {0};
    assert(btree_status_ok(btree_peek_min(&tree, &min)));
    assert(btree_status_ok(btree_peek_max(&tree, &max)));
    assert(min.key == 0u);
    assert(max.key == 19u);

    const item_t update = { .key = 10u, .value = -1 };
    assert(btree_status_ok(btree_insert(&tree, &update)));

    item_t got = {0};
    assert(btree_status_ok(btree_get(&tree, &update, &got)));
    assert(got.value == -1);

    for (uint32_t i = 0u; i < 20u; ++i) {
        const item_t key = { .key = i };
        assert(btree_status_ok(btree_remove(&tree, &key)));
    }

    assert(btree_empty(&tree));

    btree_deinit(&tree);
    arena_deinit(&arena);
}

static void test_btree_create(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    btree_t *tree = NULL;
    assert(btree_status_ok(btree_create(
        &tree,
        sizeof(item_t),
        item_compare,
        &arena,
        BTREE_FLAG_NONE
    )));

    const item_t item = { .key = 42u, .value = 100 };
    assert(btree_status_ok(btree_insert(tree, &item)));
    assert(btree_contains(tree, &item));

    btree_destroy(tree);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_dynamic_btree(void)
{
    btree_t *tree = NULL;
    assert(btree_status_ok(btree_create(
        &tree,
        sizeof(item_t),
        item_compare,
        NULL,
        BTREE_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 100u; ++i) {
        const item_t item = { .key = i, .value = (int32_t)i };
        assert(btree_status_ok(btree_insert(tree, &item)));
    }

    assert(btree_size(tree) == 100u);

    const item_t key = { .key = 50u };
    assert(btree_status_ok(btree_remove(tree, &key)));
    assert(!btree_contains(tree, &key));

    btree_destroy(tree);
}
#endif

int main(void)
{
    test_caller_owned_node_pool();
    test_arena_dynamic_nodes();
    test_btree_create();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_dynamic_btree();
#endif

    puts("btree: ok");
    return 0;
}
