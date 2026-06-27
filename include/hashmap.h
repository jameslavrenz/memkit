#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hashmap_status {
    HASHMAP_OK = 0,
    HASHMAP_ERR_NULL,
    HASHMAP_ERR_INVALID,
    HASHMAP_ERR_NOT_FOUND,
    HASHMAP_ERR_FULL,
    HASHMAP_ERR_OOM,
    HASHMAP_ERR_UNSUPPORTED,
} hashmap_status_t;

typedef enum hashmap_strategy {
    HASHMAP_STRATEGY_CHAINING = 0,
    HASHMAP_STRATEGY_OPEN_ADDRESSING = 1,
} hashmap_strategy_t;

typedef enum hashmap_flag : unsigned {
    HASHMAP_FLAG_NONE            = 0u,
    HASHMAP_FLAG_OWNS_STORAGE    = 1u << 0u, /* hashmap frees bucket/slot storage on deinit */
    HASHMAP_FLAG_OWNS_SELF       = 1u << 1u, /* hashmap struct was heap/arena allocated */
    HASHMAP_FLAG_DYNAMIC_STORAGE = 1u << 2u, /* bucket/slot storage came from heap (if enabled) */
    HASHMAP_FLAG_ARENA_STORAGE   = 1u << 3u, /* bucket/slot storage came from an arena */
    HASHMAP_FLAG_GROWABLE        = 1u << 4u, /* rehash when load factor exceeded */
} hashmap_flag_t;

typedef size_t (*hashmap_hash_fn)(const void *key, size_t key_size, void *user);
typedef bool (*hashmap_key_eq_fn)(const void *a, const void *b, size_t key_size, void *user);
typedef hashmap_status_t (*hashmap_copy_fn)(void *dst, const void *src, void *user);
typedef void (*hashmap_destroy_fn)(void *item, void *user);
typedef hashmap_status_t (*hashmap_visit_fn)(
    const void *key,
    const void *value,
    void *user
);

typedef struct hashmap_node {
    struct hashmap_node *next;
#ifdef __cplusplus
    uint8_t data[1];
#else
    uint8_t data[];
#endif
} hashmap_node_t;

typedef struct hashmap {
    alignas(max_align_t) unsigned char bytes[MEMKIT_HASHMAP_OBJ_BYTES];
} hashmap_t;

typedef struct hashmap_config {
    size_t key_size;
    size_t value_size;
    size_t bucket_count;
    hashmap_strategy_t strategy;

    /* Caller-owned bucket array or open-addressing slot storage. */
    void *storage;
    size_t storage_bytes;

    arena_t *arena;

    hashmap_hash_fn hash_fn;
    hashmap_key_eq_fn key_eq_fn;
    hashmap_copy_fn copy_key_fn;
    hashmap_copy_fn copy_value_fn;
    hashmap_destroy_fn destroy_key_fn;
    hashmap_destroy_fn destroy_value_fn;
    void *user;

    unsigned flags;
} hashmap_config_t;

[[nodiscard]] size_t hashmap_hash_bytes(const void *key, size_t key_size, void *user);
[[nodiscard]] bool hashmap_key_equal_bytes(
    const void *a,
    const void *b,
    size_t key_size,
    void *user
);

/* Size of one open-addressing slot; useful for caller-owned static storage. */
[[nodiscard]] size_t hashmap_open_slot_stride(size_t key_size, size_t value_size);

[[nodiscard]] hashmap_status_t hashmap_init(hashmap_t *map, const hashmap_config_t *config);
[[nodiscard]] hashmap_status_t hashmap_create(
    hashmap_t **map,
    size_t key_size,
    size_t value_size,
    size_t initial_buckets,
    hashmap_strategy_t strategy,
    arena_t *arena,
    unsigned flags
);
void hashmap_deinit(hashmap_t *map);
void hashmap_destroy(hashmap_t *map);

[[nodiscard]] hashmap_strategy_t hashmap_strategy_of(const hashmap_t *map);
[[nodiscard]] size_t hashmap_size(const hashmap_t *map);
[[nodiscard]] size_t hashmap_bucket_count(const hashmap_t *map);
[[nodiscard]] bool hashmap_empty(const hashmap_t *map);

void hashmap_clear(hashmap_t *map);

[[nodiscard]] hashmap_status_t hashmap_put(
    hashmap_t *map,
    const void *key,
    const void *value
);
[[nodiscard]] hashmap_status_t hashmap_get(
    const hashmap_t *map,
    const void *key,
    void *out_value
);
[[nodiscard]] hashmap_status_t hashmap_remove(hashmap_t *map, const void *key);
[[nodiscard]] bool hashmap_contains(const hashmap_t *map, const void *key);

[[nodiscard]] hashmap_status_t hashmap_foreach(const hashmap_t *map, hashmap_visit_fn visit, void *user);

[[nodiscard]] static inline bool hashmap_status_ok(hashmap_status_t status)
{
    return status == HASHMAP_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* HASHMAP_H */
