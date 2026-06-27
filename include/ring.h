#ifndef RING_H
#define RING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ring_status {
    RING_OK = 0,
    RING_ERR_NULL,
    RING_ERR_INVALID,
    RING_ERR_EMPTY,
    RING_ERR_FULL,
    RING_ERR_OOM,
    RING_ERR_UNSUPPORTED,
} ring_status_t;

typedef enum ring_flag : unsigned {
    RING_FLAG_NONE              = 0u,
    RING_FLAG_OWNS_STORAGE      = 1u << 0u,
    RING_FLAG_OWNS_SELF         = 1u << 1u,
    RING_FLAG_DYNAMIC_STORAGE   = 1u << 2u,
    RING_FLAG_ARENA_STORAGE     = 1u << 3u,
    RING_FLAG_OVERWRITE_ON_FULL = 1u << 4u,
} ring_flag_t;

typedef ring_status_t (*ring_copy_fn)(void *dst, const void *src, void *user);
typedef void (*ring_destroy_fn)(void *elem, void *user);
typedef ring_status_t (*ring_visit_fn)(const void *elem, size_t index, void *user);

typedef struct ring {
    alignas(max_align_t) unsigned char bytes[MEMKIT_RING_OBJ_BYTES];
} ring_t;

typedef struct ring_config {
    size_t elem_size;
    size_t capacity;

    void *storage;
    size_t storage_bytes;

    arena_t *arena;

    ring_copy_fn copy_fn;
    ring_destroy_fn destroy_fn;
    void *user;

    unsigned flags;
} ring_config_t;

[[nodiscard]] ring_status_t ring_init(ring_t *ring, const ring_config_t *config);
[[nodiscard]] ring_status_t ring_create(
    ring_t **ring,
    size_t elem_size,
    size_t capacity,
    arena_t *arena,
    unsigned flags
);
void ring_deinit(ring_t *ring);
void ring_destroy(ring_t *ring);

[[nodiscard]] size_t ring_size(const ring_t *ring);
[[nodiscard]] size_t ring_capacity(const ring_t *ring);
[[nodiscard]] bool ring_empty(const ring_t *ring);
[[nodiscard]] bool ring_full(const ring_t *ring);

void ring_clear(ring_t *ring);

[[nodiscard]] ring_status_t ring_push_back(ring_t *ring, const void *elem);
[[nodiscard]] ring_status_t ring_push_front(ring_t *ring, const void *elem);
[[nodiscard]] ring_status_t ring_pop_front(ring_t *ring, void *out_elem);
[[nodiscard]] ring_status_t ring_pop_back(ring_t *ring, void *out_elem);

[[nodiscard]] ring_status_t ring_peek_front(const ring_t *ring, void *out_elem);
[[nodiscard]] ring_status_t ring_peek_back(const ring_t *ring, void *out_elem);
[[nodiscard]] ring_status_t ring_peek_at(const ring_t *ring, size_t index, void *out_elem);

[[nodiscard]] ring_status_t ring_set_at(ring_t *ring, size_t index, const void *elem);

[[nodiscard]] ring_status_t ring_foreach(const ring_t *ring, ring_visit_fn visit, void *user);

[[nodiscard]] size_t ring_readable_contiguous(const ring_t *ring, const void **out_ptr);
[[nodiscard]] size_t ring_writable_contiguous(ring_t *ring, void **out_ptr);
void ring_commit_read(ring_t *ring, size_t elem_count);
void ring_commit_write(ring_t *ring, size_t elem_count);

[[nodiscard]] static inline bool ring_status_ok(ring_status_t status)
{
    return status == RING_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* RING_H */
