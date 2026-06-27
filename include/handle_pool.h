#ifndef HANDLE_POOL_H
#define HANDLE_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "arena_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum handle_pool_status {
    HANDLE_POOL_OK = 0,
    HANDLE_POOL_ERR_NULL,
    HANDLE_POOL_ERR_INVALID,
    HANDLE_POOL_ERR_EMPTY,
    HANDLE_POOL_ERR_FULL,
    HANDLE_POOL_ERR_OOM,
    HANDLE_POOL_ERR_UNSUPPORTED,
} handle_pool_status_t;

typedef enum handle_pool_flag : unsigned {
    HANDLE_POOL_FLAG_NONE            = 0u,
    HANDLE_POOL_FLAG_OWNS_STORAGE    = 1u << 0u,
    HANDLE_POOL_FLAG_OWNS_META       = 1u << 1u,
    HANDLE_POOL_FLAG_OWNS_SELF       = 1u << 2u,
    HANDLE_POOL_FLAG_DYNAMIC_STORAGE = 1u << 3u,
    HANDLE_POOL_FLAG_ARENA_STORAGE   = 1u << 4u,
    HANDLE_POOL_FLAG_FIXED_CAPACITY  = 1u << 5u,
} handle_pool_flag_t;

typedef uint32_t handle_t;

#define HANDLE_POOL_INVALID_HANDLE 0u

typedef struct handle_pool {
    alignas(max_align_t) unsigned char bytes[MEMKIT_HANDLE_POOL_OBJ_BYTES];
} handle_pool_t;

typedef struct handle_pool_config {
    size_t elem_size;
    size_t capacity;

    void *storage;
    size_t storage_bytes;

    uint16_t *generations;
    size_t generations_bytes;

    uint32_t *free_stack;
    size_t free_stack_bytes;

    arena_t *arena;

    unsigned flags;
} handle_pool_config_t;

[[nodiscard]] size_t handle_pool_generations_bytes(size_t capacity);
[[nodiscard]] size_t handle_pool_free_stack_bytes(size_t capacity);
[[nodiscard]] size_t handle_pool_storage_bytes(size_t elem_size, size_t capacity);

[[nodiscard]] handle_pool_status_t handle_pool_init(
    handle_pool_t *pool,
    const handle_pool_config_t *config
);
[[nodiscard]] handle_pool_status_t handle_pool_create(
    handle_pool_t **pool,
    size_t elem_size,
    size_t capacity,
    arena_t *arena,
    unsigned flags
);
void handle_pool_deinit(handle_pool_t *pool);
void handle_pool_destroy(handle_pool_t *pool);

[[nodiscard]] size_t handle_pool_size(const handle_pool_t *pool);
[[nodiscard]] size_t handle_pool_capacity(const handle_pool_t *pool);
[[nodiscard]] size_t handle_pool_available(const handle_pool_t *pool);
[[nodiscard]] bool handle_pool_empty(const handle_pool_t *pool);
[[nodiscard]] bool handle_pool_full(const handle_pool_t *pool);

[[nodiscard]] handle_pool_status_t handle_pool_acquire(
    handle_pool_t *pool,
    void **out_elem,
    handle_t *out_handle
);
[[nodiscard]] handle_pool_status_t handle_pool_release(handle_pool_t *pool, handle_t handle);
[[nodiscard]] handle_pool_status_t handle_pool_get(
    const handle_pool_t *pool,
    handle_t handle,
    void **out_elem
);
[[nodiscard]] bool handle_pool_valid(const handle_pool_t *pool, handle_t handle);

[[nodiscard]] static inline bool handle_pool_status_ok(handle_pool_status_t status)
{
    return status == HANDLE_POOL_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* HANDLE_POOL_H */
