#ifndef OBJPOOL_H
#define OBJPOOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum objpool_status {
    OBJPOOL_OK = 0,
    OBJPOOL_ERR_NULL,
    OBJPOOL_ERR_INVALID,
    OBJPOOL_ERR_NOT_FOUND,
    OBJPOOL_ERR_EMPTY,
    OBJPOOL_ERR_FULL,
    OBJPOOL_ERR_OOM,
    OBJPOOL_ERR_UNSUPPORTED,
} objpool_status_t;

typedef enum objpool_flag : unsigned {
    OBJPOOL_FLAG_NONE            = 0u,
    OBJPOOL_FLAG_OWNS_STORAGE    = 1u << 0u, /* pool frees element slab on deinit */
    OBJPOOL_FLAG_OWNS_META       = 1u << 1u, /* pool frees free_stack and used_bits */
    OBJPOOL_FLAG_OWNS_SELF       = 1u << 2u,
    OBJPOOL_FLAG_DYNAMIC_STORAGE = 1u << 3u,
    OBJPOOL_FLAG_ARENA_STORAGE   = 1u << 4u,
    OBJPOOL_FLAG_FIXED_CAPACITY  = 1u << 5u, /* alloc fails when full */
} objpool_flag_t;

typedef objpool_status_t (*objpool_copy_fn)(void *dst, const void *src, void *user);
typedef void (*objpool_destroy_fn)(void *elem, void *user);
typedef objpool_status_t (*objpool_visit_fn)(const void *elem, size_t index, void *user);

typedef struct objpool {
    alignas(max_align_t) unsigned char bytes[MEMKIT_OBJPOOL_OBJ_BYTES];
} objpool_t;

typedef struct objpool_config {
    size_t elem_size;
    size_t capacity;

    void *storage;              /* element slab; objpool_storage_bytes(elem_size, cap) */
    size_t storage_bytes;

    uint32_t *free_stack;       /* index stack; objpool_free_stack_bytes(capacity) */
    size_t free_stack_bytes;

    uint8_t *used_bits;         /* live-slot bitmap; objpool_used_bits_bytes(capacity) */
    size_t used_bits_bytes;

    arena_t *arena;

    objpool_copy_fn copy_fn;
    objpool_destroy_fn destroy_fn;
    void *user;

    unsigned flags;
} objpool_config_t;

[[nodiscard]] size_t objpool_used_bits_bytes(size_t capacity);
[[nodiscard]] size_t objpool_free_stack_bytes(size_t capacity);
[[nodiscard]] size_t objpool_storage_bytes(size_t elem_size, size_t capacity);

[[nodiscard]] objpool_status_t objpool_init(objpool_t *pool, const objpool_config_t *config);
[[nodiscard]] objpool_status_t objpool_create(
    objpool_t **pool,
    size_t elem_size,
    size_t capacity,
    arena_t *arena,
    unsigned flags
);
void objpool_deinit(objpool_t *pool);
void objpool_destroy(objpool_t *pool);

[[nodiscard]] size_t objpool_size(const objpool_t *pool);
[[nodiscard]] size_t objpool_capacity(const objpool_t *pool);
[[nodiscard]] size_t objpool_available(const objpool_t *pool);
[[nodiscard]] bool objpool_empty(const objpool_t *pool);
[[nodiscard]] bool objpool_full(const objpool_t *pool);

void objpool_clear(objpool_t *pool);

[[nodiscard]] objpool_status_t objpool_alloc(objpool_t *pool, void **out_elem);
[[nodiscard]] objpool_status_t objpool_alloc_copy(
    objpool_t *pool,
    const void *src,
    void **out_elem
);
[[nodiscard]] objpool_status_t objpool_free(objpool_t *pool, void *elem);

[[nodiscard]] bool objpool_contains(const objpool_t *pool, const void *elem);
[[nodiscard]] objpool_status_t objpool_index(const objpool_t *pool, const void *elem, size_t *out_index);

[[nodiscard]] objpool_status_t objpool_foreach(const objpool_t *pool, objpool_visit_fn visit, void *user);

[[nodiscard]] static inline bool objpool_status_ok(objpool_status_t status)
{
    return status == OBJPOOL_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* OBJPOOL_H */
