#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "memkit_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum arena_status {
    ARENA_OK = 0,
    ARENA_ERR_NULL,
    ARENA_ERR_INVALID,
    ARENA_ERR_OOM,
    ARENA_ERR_UNSUPPORTED,
} arena_status_t;

typedef enum arena_flag : unsigned {
    ARENA_FLAG_NONE             = 0u,
    ARENA_FLAG_OWNS_BACKING     = 1u << 0u, /* arena frees backing on deinit   */
    ARENA_FLAG_DYNAMIC_BACKING  = 1u << 1u, /* backing came from heap (if enabled) */
    ARENA_FLAG_MMAP_BACKING     = 1u << 2u, /* backing came from mmap (MPU only) */
} arena_flag_t;

typedef enum arena_backing {
    ARENA_BACKING_USER_BUFFER = MEMKIT_MEMORY_FIXED_BUFFER,
    ARENA_BACKING_HEAP        = MEMKIT_MEMORY_HEAP,
    ARENA_BACKING_MMAP        = MEMKIT_MEMORY_MMAP,
} arena_backing_t;

typedef struct arena {
    uint8_t *base;
    size_t capacity_bytes;
    size_t offset_bytes;
    size_t allocation_count;
    unsigned flags;
} arena_t;

typedef struct arena_config {
    void *backing;
    size_t backing_bytes;
    unsigned flags;
} arena_config_t;

typedef struct arena_stats {
    size_t capacity_bytes;
    size_t used_bytes;
    size_t remaining_bytes;
    size_t allocation_count;
} arena_stats_t;

[[nodiscard]] arena_status_t arena_init(arena_t *arena, const arena_config_t *config);
[[nodiscard]] arena_status_t arena_create(arena_t **arena, size_t backing_bytes);
[[nodiscard]] arena_status_t arena_create_with_backing(
    arena_t **arena,
    size_t backing_bytes,
    arena_backing_t backing
);
void arena_deinit(arena_t *arena);
void arena_destroy(arena_t *arena);

void arena_reset(arena_t *arena);

[[nodiscard]] arena_status_t arena_alloc(
    arena_t *arena,
    size_t size,
    size_t alignment,
    void **out_ptr
);

[[nodiscard]] arena_status_t arena_calloc(
    arena_t *arena,
    size_t count,
    size_t size,
    size_t alignment,
    void **out_ptr
);

[[nodiscard]] arena_status_t arena_stats(const arena_t *arena, arena_stats_t *out_stats);

[[nodiscard]] static inline bool arena_status_ok(arena_status_t status)
{
    return status == ARENA_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* ARENA_H */
