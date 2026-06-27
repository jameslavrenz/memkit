#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "lrucache.h"

typedef struct {
    uint32_t *keys;
    size_t count;
} key_collect_t;

static lrucache_status_t collect_keys_mru(
    const void *key,
    const void *value,
    void *user
)
{
    (void)value;
    key_collect_t *const ctx = (key_collect_t *)user;
    ctx->keys[ctx->count] = *(const uint32_t *)key;
    ctx->count++;
    return LRUCACHE_OK;
}

static void test_lrucache_caller_owned(void)
{
    enum { CAPACITY = 3u, BUCKETS = 4u };

    static uint8_t entry_pool[128];
    static lrucache_entry_t *buckets[BUCKETS];

    const size_t stride = lrucache_entry_stride(sizeof(uint32_t), sizeof(int32_t));
    assert(lrucache_entry_pool_bytes(CAPACITY, sizeof(uint32_t), sizeof(int32_t)) <=
           sizeof entry_pool);

    lrucache_t cache;
    assert(lrucache_status_ok(lrucache_init(&cache, &(lrucache_config_t){
        .key_size = sizeof(uint32_t),
        .value_size = sizeof(int32_t),
        .capacity = CAPACITY,
        .bucket_count = BUCKETS,
        .entry_pool = entry_pool,
        .entry_pool_bytes = CAPACITY * stride,
        .buckets = buckets,
        .buckets_bytes = sizeof buckets,
    })));

    for (uint32_t key = 1u; key <= 3u; ++key) {
        const int32_t value = (int32_t)(key * 10);
        assert(lrucache_status_ok(lrucache_put(&cache, &key, &value)));
    }

    assert(lrucache_full(&cache));

    int32_t value = 0;
    assert(lrucache_status_ok(lrucache_get(&cache, &((uint32_t){1u}), &value)));
    assert(value == 10);

    const uint32_t key4 = 4u;
    const int32_t value4 = 40;
    assert(lrucache_status_ok(lrucache_put(&cache, &key4, &value4)));

    assert(!lrucache_contains(&cache, &((uint32_t){2u})));
    assert(lrucache_contains(&cache, &((uint32_t){1u})));
    assert(lrucache_contains(&cache, &((uint32_t){3u})));
    assert(lrucache_contains(&cache, &key4));

    uint32_t order[3] = {0};
    key_collect_t collected = { .keys = order, .count = 0u };
    assert(lrucache_status_ok(lrucache_foreach_mru(&cache, collect_keys_mru, &collected)));
    assert(collected.count == 3u);
    assert(order[0] == 4u);
    assert(order[1] == 1u);
    assert(order[2] == 3u);

    lrucache_deinit(&cache);
}

static void test_lrucache_peek_no_promote(void)
{
    enum { CAPACITY = 3u, BUCKETS = 4u };

    static uint8_t entry_pool[128];
    static lrucache_entry_t *buckets[BUCKETS];

    const size_t stride = lrucache_entry_stride(sizeof(uint32_t), sizeof(int32_t));

    lrucache_t cache;
    assert(lrucache_status_ok(lrucache_init(&cache, &(lrucache_config_t){
        .key_size = sizeof(uint32_t),
        .value_size = sizeof(int32_t),
        .capacity = CAPACITY,
        .bucket_count = BUCKETS,
        .entry_pool = entry_pool,
        .entry_pool_bytes = CAPACITY * stride,
        .buckets = buckets,
        .buckets_bytes = sizeof buckets,
    })));

    for (uint32_t key = 1u; key <= 3u; ++key) {
        const int32_t value = (int32_t)key;
        assert(lrucache_status_ok(lrucache_put(&cache, &key, &value)));
    }

    const uint32_t key3 = 3u;
    int32_t out = 0;
    assert(lrucache_status_ok(lrucache_peek(&cache, &key3, &out)));
    assert(out == 3);

    const uint32_t key4 = 4u;
    const int32_t value4 = 4;
    assert(lrucache_status_ok(lrucache_put(&cache, &key4, &value4)));
    assert(!lrucache_contains(&cache, &((uint32_t){1u})));

    lrucache_deinit(&cache);
}

static void test_lrucache_arena(void)
{
    static uint8_t arena_backing[8192];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    lrucache_t *cache = NULL;
    assert(lrucache_status_ok(lrucache_create(
        &cache,
        sizeof(uint32_t),
        sizeof(int32_t),
        8u,
        0u,
        &arena,
        LRUCACHE_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 8u; ++i) {
        const int32_t value = (int32_t)i;
        assert(lrucache_status_ok(lrucache_put(cache, &i, &value)));
    }

    assert(lrucache_full(cache));

    const uint32_t touch_key = 0u;
    assert(lrucache_status_ok(lrucache_touch(cache, &touch_key)));

    const uint32_t new_key = 99u;
    const int32_t new_value = 990;
    assert(lrucache_status_ok(lrucache_put(cache, &new_key, &new_value)));
    assert(lrucache_contains(cache, &touch_key));
    assert(lrucache_contains(cache, &new_key));

    lrucache_destroy(cache);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_lrucache_dynamic(void)
{
    lrucache_t *cache = NULL;
    assert(lrucache_status_ok(lrucache_create(
        &cache,
        sizeof(uint32_t),
        sizeof(int32_t),
        16u,
        32u,
        NULL,
        LRUCACHE_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 32u; ++i) {
        const int32_t value = (int32_t)i;
        assert(lrucache_status_ok(lrucache_put(cache, &i, &value)));
    }

    assert(lrucache_size(cache) == 16u);

    for (uint32_t i = 16u; i < 32u; ++i) {
        assert(lrucache_contains(cache, &i));
    }

    for (uint32_t i = 0u; i < 16u; ++i) {
        assert(!lrucache_contains(cache, &i));
    }

    lrucache_destroy(cache);
}
#endif

int main(void)
{
    test_lrucache_caller_owned();
    test_lrucache_peek_no_promote();
    test_lrucache_arena();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_lrucache_dynamic();
#endif

    puts("lrucache: ok");
    return 0;
}
