#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "hashmap.h"

static hashmap_status_t visit_count(
    const void *key,
    const void *value,
    void *user
)
{
    (void)key;
    (void)value;
    size_t *count = (size_t *)user;
    (*count)++;
    return HASHMAP_OK;
}

static void test_strategy(
    hashmap_strategy_t strategy,
    const char *name
)
{
    static uint8_t arena_backing[16384];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    hashmap_t *map = NULL;
    assert(hashmap_status_ok(hashmap_create(
        &map,
        sizeof(uint32_t),
        sizeof(int32_t),
        4u,
        strategy,
        &arena,
        HASHMAP_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 32u; ++i) {
        const int32_t value = (int32_t)(i * 10);
        assert(hashmap_status_ok(hashmap_put(map, &i, &value)));
    }

    assert(hashmap_size(map) == 32u);
    assert(hashmap_bucket_count(map) >= 4u);

    for (uint32_t i = 0u; i < 32u; ++i) {
        assert(hashmap_contains(map, &i));
        int32_t out = 0;
        assert(hashmap_status_ok(hashmap_get(map, &i, &out)));
        assert(out == (int32_t)(i * 10));
    }

    const uint32_t missing_key = 999u;
    int32_t missing_value = 0;
    assert(!hashmap_contains(map, &missing_key));
    assert(hashmap_get(map, &missing_key, &missing_value) == HASHMAP_ERR_NOT_FOUND);

    for (uint32_t i = 0u; i < 16u; ++i) {
        assert(hashmap_status_ok(hashmap_remove(map, &i)));
    }

    assert(hashmap_size(map) == 16u);

    const uint32_t removed_key = 0u;
    assert(!hashmap_contains(map, &removed_key));

    const int32_t updated = -1;
    const uint32_t key = 16u;
    assert(hashmap_status_ok(hashmap_put(map, &key, &updated)));

    int32_t got = 0;
    assert(hashmap_status_ok(hashmap_get(map, &key, &got)));
    assert(got == -1);

    size_t visited = 0u;
    assert(hashmap_status_ok(hashmap_foreach(map, visit_count, &visited)));
    assert(visited == 16u);

    hashmap_destroy(map);
    arena_deinit(&arena);

    printf("hashmap %s: ok\n", name);
}

static void test_caller_owned_open_addressing(void)
{
    enum { BUCKET_COUNT = 8u };
    const size_t slot_stride = hashmap_open_slot_stride(sizeof(uint32_t), sizeof(int32_t));
    static uint8_t storage[BUCKET_COUNT * 32u];

    hashmap_t map;
    assert(hashmap_status_ok(hashmap_init(&map, &(hashmap_config_t){
        .key_size = sizeof(uint32_t),
        .value_size = sizeof(int32_t),
        .bucket_count = BUCKET_COUNT,
        .strategy = HASHMAP_STRATEGY_OPEN_ADDRESSING,
        .storage = storage,
        .storage_bytes = BUCKET_COUNT * slot_stride,
    })));

    for (uint32_t i = 0u; i < BUCKET_COUNT; ++i) {
        const int32_t value = (int32_t)i;
        assert(hashmap_status_ok(hashmap_put(&map, &i, &value)));
    }

    assert(hashmap_size(&map) == BUCKET_COUNT);

    const uint32_t extra_key = 99u;
    const int32_t extra_value = 99;
    assert(hashmap_put(&map, &extra_key, &extra_value) == HASHMAP_ERR_FULL);

    hashmap_deinit(&map);
}

static void test_caller_owned_chaining(void)
{
    enum { BUCKET_COUNT = 8u };
    static hashmap_node_t *buckets[BUCKET_COUNT];

    static uint8_t arena_backing[4096];
    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    hashmap_t map;
    assert(hashmap_status_ok(hashmap_init(&map, &(hashmap_config_t){
        .key_size = sizeof(uint32_t),
        .value_size = sizeof(int32_t),
        .bucket_count = BUCKET_COUNT,
        .strategy = HASHMAP_STRATEGY_CHAINING,
        .storage = buckets,
        .storage_bytes = sizeof buckets,
        .arena = &arena,
        .flags = HASHMAP_FLAG_GROWABLE,
    })));

    for (uint32_t i = 0u; i < 10u; ++i) {
        const int32_t value = (int32_t)i;
        assert(hashmap_status_ok(hashmap_put(&map, &i, &value)));
    }

    assert(hashmap_size(&map) == 10u);

    hashmap_deinit(&map);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_dynamic_chaining(void)
{
    hashmap_t *map = NULL;
    assert(hashmap_status_ok(hashmap_create(
        &map,
        sizeof(uint32_t),
        sizeof(int32_t),
        8u,
        HASHMAP_STRATEGY_CHAINING,
        NULL,
        HASHMAP_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 100u; ++i) {
        const int32_t value = (int32_t)i;
        assert(hashmap_status_ok(hashmap_put(map, &i, &value)));
    }

    assert(hashmap_size(map) == 100u);
    hashmap_destroy(map);
}
#endif

int main(void)
{
    test_strategy(HASHMAP_STRATEGY_CHAINING, "chaining");
    test_strategy(HASHMAP_STRATEGY_OPEN_ADDRESSING, "open addressing");
    test_caller_owned_open_addressing();
    test_caller_owned_chaining();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_dynamic_chaining();
#endif
    return 0;
}
