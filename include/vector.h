#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vector_status {
    VECTOR_OK = 0,
    VECTOR_ERR_NULL,
    VECTOR_ERR_INVALID,
    VECTOR_ERR_EMPTY,
    VECTOR_ERR_FULL,
    VECTOR_ERR_OOM,
    VECTOR_ERR_UNSUPPORTED,
} vector_status_t;

typedef enum vector_flag : unsigned {
    VECTOR_FLAG_NONE            = 0u,
    VECTOR_FLAG_OWNS_STORAGE    = 1u << 0u, /* vector frees element storage on deinit */
    VECTOR_FLAG_OWNS_SELF       = 1u << 1u, /* vector struct was heap/arena allocated */
    VECTOR_FLAG_DYNAMIC_STORAGE = 1u << 2u, /* element storage came from heap (if enabled) */
    VECTOR_FLAG_ARENA_STORAGE   = 1u << 3u, /* element storage came from an arena */
    VECTOR_FLAG_GROWABLE        = 1u << 4u, /* double capacity when full */
} vector_flag_t;

typedef vector_status_t (*vector_copy_fn)(void *dst, const void *src, void *user);
typedef void (*vector_destroy_fn)(void *elem, void *user);
typedef vector_status_t (*vector_visit_fn)(const void *elem, size_t index, void *user);

typedef struct vector {
    alignas(max_align_t) unsigned char bytes[MEMKIT_VECTOR_OBJ_BYTES];
} vector_t;

typedef struct vector_config {
    size_t elem_size;
    size_t capacity;

    /* Caller-owned element storage (used with vector_init). */
    void *storage;
    size_t storage_bytes;

    /* Optional arena for vector-owned or growable storage without heap. */
    arena_t *arena;

    vector_copy_fn copy_fn;
    vector_destroy_fn destroy_fn;
    void *user;

    unsigned flags;
} vector_config_t;

[[nodiscard]] vector_status_t vector_init(vector_t *vector, const vector_config_t *config);
[[nodiscard]] vector_status_t vector_create(
    vector_t **vector,
    size_t elem_size,
    size_t initial_capacity,
    arena_t *arena,
    unsigned flags
);
void vector_deinit(vector_t *vector);
void vector_destroy(vector_t *vector);

[[nodiscard]] size_t vector_size(const vector_t *vector);
[[nodiscard]] size_t vector_capacity(const vector_t *vector);
[[nodiscard]] bool vector_empty(const vector_t *vector);

void vector_clear(vector_t *vector);

[[nodiscard]] vector_status_t vector_reserve(vector_t *vector, size_t min_capacity);
[[nodiscard]] vector_status_t vector_push_back(vector_t *vector, const void *elem);
[[nodiscard]] vector_status_t vector_pop_back(vector_t *vector, void *out_elem);

[[nodiscard]] vector_status_t vector_peek_front(const vector_t *vector, void *out_elem);
[[nodiscard]] vector_status_t vector_peek_back(const vector_t *vector, void *out_elem);
[[nodiscard]] vector_status_t vector_peek_at(const vector_t *vector, size_t index, void *out_elem);

[[nodiscard]] vector_status_t vector_set_at(vector_t *vector, size_t index, const void *elem);

[[nodiscard]] void *vector_at(vector_t *vector, size_t index);
[[nodiscard]] const void *vector_at_const(const vector_t *vector, size_t index);

[[nodiscard]] vector_status_t vector_foreach(const vector_t *vector, vector_visit_fn visit, void *user);

[[nodiscard]] static inline bool vector_status_ok(vector_status_t status)
{
    return status == VECTOR_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* VECTOR_H */
