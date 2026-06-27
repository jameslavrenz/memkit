#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "lrucache.h"

static size_t hash_u32(const void *key, size_t key_size, void *user)
{
    (void)user;
    (void)key_size;
    return (size_t)(*(const uint32_t *)key);
}

static bool key_eq_u32(const void *a, const void *b, size_t key_size, void *user)
{
    (void)user;
    (void)key_size;
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

int main(void)
{
    lrucache_t cache;
    assert(lrucache_status_ok(lrucache_init(&cache, &(lrucache_config_t){
        .key_size   = sizeof(uint32_t),
        .value_size = sizeof(int32_t),
        .capacity   = 4u,
        .hash_fn    = hash_u32,
        .key_eq_fn  = key_eq_u32,
        .flags      = LRUCACHE_FLAG_DYNAMIC_STORAGE,
    })));

    for (uint32_t i = 0u; i < 4u; ++i) {
        const int32_t value = (int32_t)(i * 10);
        assert(lrucache_status_ok(lrucache_put(&cache, &i, &value)));
    }

    assert(lrucache_size(&cache) == 4u);

    int32_t got = 0;
    const uint32_t key2 = 2u;
    assert(lrucache_status_ok(lrucache_get(&cache, &key2, &got)));
    assert(got == 20);

    assert(lrucache_status_ok(lrucache_touch(&cache, &key2)));

    const uint32_t key5 = 5u;
    const int32_t value50 = 50;
    assert(lrucache_status_ok(lrucache_put(&cache, &key5, &value50)));
    assert(lrucache_size(&cache) == 4u);

    assert(lrucache_status_ok(lrucache_remove(&cache, &key5)));
    assert(!lrucache_contains(&cache, &key5));

    lrucache_clear(&cache);
    assert(lrucache_empty(&cache));
    lrucache_deinit(&cache);

    printf("lrucache_c: ok\n");
    return 0;
}
