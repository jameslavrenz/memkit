#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "btree.h"

typedef struct kv {
    uint32_t key;
    int32_t value;
} kv_t;

static int compare_kv(const void *a, const void *b, void *user)
{
    (void)user;
    const kv_t *left = (const kv_t *)a;
    const kv_t *right = (const kv_t *)b;
    if (left->key < right->key) {
        return -1;
    }
    if (left->key > right->key) {
        return 1;
    }
    return 0;
}

int main(void)
{
    btree_t tree;
    assert(btree_status_ok(btree_init(&tree, &(btree_config_t){
        .elem_size     = sizeof(kv_t),
        .node_capacity = 16u,
        .compare_fn    = compare_kv,
        .flags         = BTREE_FLAG_DYNAMIC_STORAGE,
    })));

    for (uint32_t i = 0u; i < 8u; ++i) {
        const kv_t kv = {.key = i, .value = (int32_t)(i * 10)};
        assert(btree_status_ok(btree_insert(&tree, &kv)));
    }

    assert(btree_size(&tree) == 8u);

    for (uint32_t i = 0u; i < 8u; ++i) {
        assert(btree_contains(&tree, &(kv_t){.key = i}));
        kv_t out = {0};
        assert(btree_status_ok(btree_get(&tree, &(kv_t){.key = i}, &out)));
        assert(out.value == (int32_t)(i * 10));
    }

    assert(btree_status_ok(btree_remove(&tree, &(kv_t){.key = 3u})));
    assert(!btree_contains(&tree, &(kv_t){.key = 3u}));
    assert(btree_size(&tree) == 7u);

    kv_t min_kv = {0};
    kv_t max_kv = {0};
    assert(btree_status_ok(btree_peek_min(&tree, &min_kv)));
    assert(btree_status_ok(btree_peek_max(&tree, &max_kv)));
    assert(min_kv.key <= max_kv.key);

    btree_deinit(&tree);
    printf("btree_c: ok\n");
    return 0;
}
