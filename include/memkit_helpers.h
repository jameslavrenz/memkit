#ifndef MEMKIT_HELPERS_H
#define MEMKIT_HELPERS_H

/*
 * C API helpers: storage sizing, static init macros, early-return checks.
 * Included via <memkit.h>; does not add code to the linked library.
 */

#include "arena.h"
#include "bitset.h"
#include "handle_pool.h"
#include "objpool.h"
#include "queue.h"
#include "ring.h"
#include "stack.h"
#include "vector.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Storage sizing ------------------------------------------------------- */

/** Element slab size in bytes (ring, queue, vector, stack). */
[[nodiscard]] static inline size_t memkit_elem_storage_bytes(size_t elem_size, size_t capacity)
{
    return elem_size * capacity;
}

#define MEMKIT_ELEM_STORAGE_BYTES(type, capacity) \
    memkit_elem_storage_bytes(sizeof(type), (capacity))

/**
 * Declare a properly aligned, caller-owned element buffer.
 * Pass the buffer name to MEMKIT_*_INIT_STATIC macros; capacity is inferred.
 */
#define MEMKIT_ELEM_STORAGE(type, capacity, name) alignas(type) type name[(capacity)]

/* --- Early return --------------------------------------------------------- */

/** Return from void function when status is not OK. */
#define MEMKIT_RETURN_IF_NOT_OK(status_expr)          \
    do {                                              \
        const int _memkit_st = (int)(status_expr);    \
        if (_memkit_st != 0) {                        \
            return;                                   \
        }                                             \
    } while (0)

/** Return a value when status is not OK (status must be 0 on success). */
#define MEMKIT_RETURN_VAL_IF_NOT_OK(status_expr, ret_val) \
    do {                                                  \
        const int _memkit_st = (int)(status_expr);        \
        if (_memkit_st != 0) {                            \
            return (ret_val);                             \
        }                                                 \
    } while (0)

/* --- Static init (tier 1) ------------------------------------------------- */

#define MEMKIT_ARENA_INIT_STATIC(arena_ptr, backing) \
    arena_init(                                      \
        (arena_ptr),                                 \
        &(arena_config_t){                           \
            .backing       = (backing),              \
            .backing_bytes = sizeof(backing),        \
        })

#define MEMKIT_RING_INIT_STATIC(ring_ptr, type, buf) \
    ring_init(                                       \
        (ring_ptr),                                  \
        &(ring_config_t){                            \
            .elem_size     = sizeof(type),           \
            .capacity      = sizeof(buf) / sizeof(type), \
            .storage       = (buf),                  \
            .storage_bytes = sizeof(buf),            \
        })

#define MEMKIT_RING_INIT_STATIC_OVERWRITE(ring_ptr, type, buf) \
    ring_init(                                                 \
        (ring_ptr),                                            \
        &(ring_config_t){                                      \
            .elem_size     = sizeof(type),                     \
            .capacity      = sizeof(buf) / sizeof(type),       \
            .storage       = (buf),                            \
            .storage_bytes = sizeof(buf),                      \
            .flags         = RING_FLAG_OVERWRITE_ON_FULL,       \
        })

#define MEMKIT_QUEUE_INIT_STATIC(queue_ptr, type, buf) \
    queue_init(                                        \
        (queue_ptr),                                   \
        &(queue_config_t){                             \
            .elem_size     = sizeof(type),             \
            .capacity      = sizeof(buf) / sizeof(type), \
            .storage       = (buf),                    \
            .storage_bytes = sizeof(buf),              \
        })

#define MEMKIT_VECTOR_INIT_STATIC(vector_ptr, type, buf) \
    vector_init(                                         \
        (vector_ptr),                                    \
        &(vector_config_t){                              \
            .elem_size     = sizeof(type),               \
            .capacity      = sizeof(buf) / sizeof(type), \
            .storage       = (buf),                      \
            .storage_bytes = sizeof(buf),                \
        })

#define MEMKIT_STACK_INIT_STATIC(stack_ptr, type, buf) \
    stack_init(                                        \
        (stack_ptr),                                   \
        &(stack_config_t){                             \
            .elem_size     = sizeof(type),             \
            .capacity      = sizeof(buf) / sizeof(type), \
            .storage       = (buf),                    \
            .storage_bytes = sizeof(buf),              \
        })

#define MEMKIT_BITSET_INIT_STATIC(bitset_ptr, capacity, buf) \
    bitset_init(                                             \
        (bitset_ptr),                                        \
        &(bitset_config_t){                                  \
            .capacity      = (capacity),                     \
            .storage       = (buf),                          \
            .storage_bytes = sizeof(buf),                    \
        })

/**
 * Declare objpool backing for static init. Use with MEMKIT_OBJPOOL_INIT_STATIC.
 * Example: MEMKIT_OBJPOOL_STORAGE(block_t, 8, pool_buf);
 */
#define MEMKIT_OBJPOOL_STORAGE(type, capacity, prefix)                          \
    alignas(type) uint8_t prefix##_storage[objpool_storage_bytes(               \
        sizeof(type),                                                           \
        (capacity))];                                                           \
    uint32_t prefix##_free_stack[(capacity)];                                   \
    uint8_t prefix##_used_bits[objpool_used_bits_bytes(capacity)]

#define MEMKIT_OBJPOOL_INIT_STATIC(pool_ptr, type, capacity, prefix) \
    objpool_init(                                                    \
        (pool_ptr),                                                    \
        &(objpool_config_t){                                         \
            .elem_size        = sizeof(type),                        \
            .capacity         = (capacity),                          \
            .storage          = prefix##_storage,                    \
            .storage_bytes    = sizeof prefix##_storage,             \
            .free_stack       = prefix##_free_stack,                 \
            .free_stack_bytes = sizeof prefix##_free_stack,          \
            .used_bits        = prefix##_used_bits,                  \
            .used_bits_bytes  = sizeof prefix##_used_bits,            \
        })

/**
 * Declare handle-pool backing for static init. Use with MEMKIT_HANDLE_POOL_INIT_STATIC.
 */
#define MEMKIT_HANDLE_POOL_STORAGE(type, capacity, prefix)                       \
    alignas(type) uint8_t prefix##_storage[handle_pool_storage_bytes(            \
        sizeof(type),                                                           \
        (capacity))];                                                             \
    uint16_t prefix##_generations[(capacity)];                                    \
    uint32_t prefix##_free_stack[(capacity)]

#define MEMKIT_HANDLE_POOL_INIT_STATIC(pool_ptr, type, capacity, prefix) \
    handle_pool_init(                                                   \
        (pool_ptr),                                                     \
        &(handle_pool_config_t){                                      \
            .elem_size          = sizeof(type),                       \
            .capacity           = (capacity),                         \
            .storage            = prefix##_storage,                   \
            .storage_bytes      = sizeof prefix##_storage,            \
            .generations        = prefix##_generations,             \
            .generations_bytes  = sizeof prefix##_generations,      \
            .free_stack           = prefix##_free_stack,            \
            .free_stack_bytes     = sizeof prefix##_free_stack,       \
        })

/* --- Debug strings (bring-up only) ---------------------------------------- */

[[nodiscard]] static inline const char *memkit_ring_status_string(ring_status_t status)
{
    switch (status) {
    case RING_OK:              return "ok";
    case RING_ERR_NULL:        return "null";
    case RING_ERR_INVALID:     return "invalid";
    case RING_ERR_EMPTY:       return "empty";
    case RING_ERR_FULL:        return "full";
    case RING_ERR_OOM:         return "oom";
    case RING_ERR_UNSUPPORTED: return "unsupported";
    default:                   return "unknown";
    }
}

[[nodiscard]] static inline const char *memkit_queue_status_string(queue_status_t status)
{
    switch (status) {
    case QUEUE_OK:              return "ok";
    case QUEUE_ERR_NULL:        return "null";
    case QUEUE_ERR_INVALID:     return "invalid";
    case QUEUE_ERR_EMPTY:       return "empty";
    case QUEUE_ERR_FULL:        return "full";
    case QUEUE_ERR_OOM:         return "oom";
    case QUEUE_ERR_UNSUPPORTED: return "unsupported";
    default:                    return "unknown";
    }
}

[[nodiscard]] static inline const char *memkit_arena_status_string(arena_status_t status)
{
    switch (status) {
    case ARENA_OK:              return "ok";
    case ARENA_ERR_NULL:        return "null";
    case ARENA_ERR_INVALID:     return "invalid";
    case ARENA_ERR_OOM:         return "oom";
    case ARENA_ERR_UNSUPPORTED: return "unsupported";
    default:                    return "unknown";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* MEMKIT_HELPERS_H */
