#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "hashmap.h"

static size_t hash_u32(const void *key, size_t key_size, void *user)
{
    (void)user;
    (void)key_size;
    const uint32_t value = *(const uint32_t *)key;
    return (size_t)(value * 2654435761u);
}

static bool key_eq_u32(const void *a, const void *b, size_t key_size, void *user)
{
    (void)user;
    (void)key_size;
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

static void test_chaining_growable(void)
{
    hashmap_t map;
    assert(hashmap_status_ok(hashmap_init(&map, &(hashmap_config_t){
        .key_size      = sizeof(uint32_t),
        .value_size    = sizeof(int32_t),
        .bucket_count  = 4u,
        .strategy      = HASHMAP_STRATEGY_CHAINING,
        .hash_fn       = hash_u32,
        .key_eq_fn     = key_eq_u32,
        .flags         = HASHMAP_FLAG_GROWABLE | HASHMAP_FLAG_DYNAMIC_STORAGE,
    })));

    for (uint32_t i = 0u; i < 32u; ++i) {
        const int32_t value = (int32_t)(i * 10);
        assert(hashmap_status_ok(hashmap_put(&map, &i, &value)));
    }

    assert(hashmap_size(&map) == 32u);

    for (uint32_t i = 0u; i < 32u; ++i) {
        assert(hashmap_contains(&map, &i));
        int32_t out = 0;
        assert(hashmap_status_ok(hashmap_get(&map, &i, &out)));
        assert(out == (int32_t)(i * 10));
    }

    const uint32_t missing_key = 999u;
    assert(!hashmap_contains(&map, &missing_key));

    for (uint32_t i = 0u; i < 16u; ++i) {
        assert(hashmap_status_ok(hashmap_remove(&map, &i)));
    }

    assert(hashmap_size(&map) == 16u);

    const uint32_t key16 = 16u;
    const int32_t value_neg1 = -1;
    assert(hashmap_status_ok(hashmap_put(&map, &key16, &value_neg1)));

    int32_t got = 0;
    assert(hashmap_status_ok(hashmap_get(&map, &key16, &got)));
    assert(got == -1);

    hashmap_deinit(&map);
}

static void test_open_addressing(void)
{
    hashmap_t map;
    assert(hashmap_status_ok(hashmap_init(&map, &(hashmap_config_t){
        .key_size     = sizeof(uint32_t),
        .value_size   = sizeof(int32_t),
        .bucket_count = 8u,
        .strategy     = HASHMAP_STRATEGY_OPEN_ADDRESSING,
        .hash_fn      = hash_u32,
        .key_eq_fn    = key_eq_u32,
        .flags        = HASHMAP_FLAG_DYNAMIC_STORAGE,
    })));

    for (uint32_t i = 0u; i < 8u; ++i) {
        const int32_t value = (int32_t)i;
        assert(hashmap_status_ok(hashmap_put(&map, &i, &value)));
    }

    assert(hashmap_size(&map) == 8u);
    hashmap_deinit(&map);
}

int main(void)
{
    test_chaining_growable();
    test_open_addressing();
    printf("hashmap_c: ok\n");
    return 0;
}
