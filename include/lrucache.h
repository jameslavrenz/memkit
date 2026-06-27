#ifndef LRUCACHE_H
#define LRUCACHE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "arena_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum lrucache_status {
    LRUCACHE_OK = 0,
    LRUCACHE_ERR_NULL,
    LRUCACHE_ERR_INVALID,
    LRUCACHE_ERR_NOT_FOUND,
    LRUCACHE_ERR_FULL,
    LRUCACHE_ERR_OOM,
    LRUCACHE_ERR_UNSUPPORTED,
} lrucache_status_t;

typedef enum lrucache_flag : unsigned {
    LRUCACHE_FLAG_NONE            = 0u,
    LRUCACHE_FLAG_OWNS_STORAGE    = 1u << 0u,
    LRUCACHE_FLAG_OWNS_SELF       = 1u << 1u,
    LRUCACHE_FLAG_DYNAMIC_STORAGE = 1u << 2u,
    LRUCACHE_FLAG_ARENA_STORAGE   = 1u << 3u,
    LRUCACHE_FLAG_FIXED_CAPACITY  = 1u << 4u,
} lrucache_flag_t;

typedef size_t (*lrucache_hash_fn)(const void *key, size_t key_size, void *user);
typedef bool (*lrucache_key_eq_fn)(const void *a, const void *b, size_t key_size, void *user);
typedef lrucache_status_t (*lrucache_copy_fn)(void *dst, const void *src, void *user);
typedef void (*lrucache_destroy_fn)(void *item, void *user);
typedef lrucache_status_t (*lrucache_visit_fn)(
    const void *key,
    const void *value,
    void *user
);

typedef struct lrucache_entry {
    struct lrucache_entry *hash_next;
    struct lrucache_entry *lru_prev;
    struct lrucache_entry *lru_next;
#ifdef __cplusplus
    uint8_t data[1];
#else
    uint8_t data[];
#endif
} lrucache_entry_t;

typedef struct lrucache {
    alignas(max_align_t) unsigned char bytes[MEMKIT_LRUCACHE_OBJ_BYTES];
} lrucache_t;

typedef struct lrucache_config {
    size_t key_size;
    size_t value_size;
    size_t capacity;
    size_t bucket_count;

    void *entry_pool;
    size_t entry_pool_bytes;

    lrucache_entry_t **buckets;
    size_t buckets_bytes;

    arena_t *arena;

    lrucache_hash_fn hash_fn;
    lrucache_key_eq_fn key_eq_fn;
    lrucache_copy_fn copy_key_fn;
    lrucache_copy_fn copy_value_fn;
    lrucache_destroy_fn destroy_key_fn;
    lrucache_destroy_fn destroy_value_fn;
    void *user;

    unsigned flags;
} lrucache_config_t;

[[nodiscard]] size_t lrucache_entry_stride(size_t key_size, size_t value_size);
[[nodiscard]] size_t lrucache_entry_pool_bytes(size_t capacity, size_t key_size, size_t value_size);
[[nodiscard]] size_t lrucache_buckets_bytes(size_t bucket_count);
[[nodiscard]] size_t lrucache_default_bucket_count(size_t capacity);

[[nodiscard]] lrucache_status_t lrucache_init(lrucache_t *cache, const lrucache_config_t *config);
[[nodiscard]] lrucache_status_t lrucache_create(
    lrucache_t **cache,
    size_t key_size,
    size_t value_size,
    size_t capacity,
    size_t bucket_count,
    arena_t *arena,
    unsigned flags
);
void lrucache_deinit(lrucache_t *cache);
void lrucache_destroy(lrucache_t *cache);

[[nodiscard]] size_t lrucache_size(const lrucache_t *cache);
[[nodiscard]] size_t lrucache_capacity(const lrucache_t *cache);
[[nodiscard]] size_t lrucache_bucket_count(const lrucache_t *cache);
[[nodiscard]] bool lrucache_empty(const lrucache_t *cache);
[[nodiscard]] bool lrucache_full(const lrucache_t *cache);

void lrucache_clear(lrucache_t *cache);

[[nodiscard]] lrucache_status_t lrucache_get(
    lrucache_t *cache,
    const void *key,
    void *out_value
);
[[nodiscard]] lrucache_status_t lrucache_peek(
    const lrucache_t *cache,
    const void *key,
    void *out_value
);
[[nodiscard]] lrucache_status_t lrucache_put(
    lrucache_t *cache,
    const void *key,
    const void *value
);
[[nodiscard]] lrucache_status_t lrucache_remove(lrucache_t *cache, const void *key);
[[nodiscard]] bool lrucache_contains(const lrucache_t *cache, const void *key);
[[nodiscard]] lrucache_status_t lrucache_touch(lrucache_t *cache, const void *key);

[[nodiscard]] lrucache_status_t lrucache_foreach_mru(
    const lrucache_t *cache,
    lrucache_visit_fn visit,
    void *user
);
[[nodiscard]] lrucache_status_t lrucache_foreach_lru(
    const lrucache_t *cache,
    lrucache_visit_fn visit,
    void *user
);

[[nodiscard]] static inline bool lrucache_status_ok(lrucache_status_t status)
{
    return status == LRUCACHE_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* LRUCACHE_H */
