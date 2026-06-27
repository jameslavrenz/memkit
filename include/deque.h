#ifndef DEQUE_H
#define DEQUE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum deque_status {
    DEQUE_OK = 0,
    DEQUE_ERR_NULL,
    DEQUE_ERR_INVALID,
    DEQUE_ERR_EMPTY,
    DEQUE_ERR_FULL,
    DEQUE_ERR_OOM,
    DEQUE_ERR_UNSUPPORTED,
} deque_status_t;

typedef enum deque_flag : unsigned {
    DEQUE_FLAG_NONE            = 0u,
    DEQUE_FLAG_OWNS_STORAGE    = 1u << 0u,
    DEQUE_FLAG_OWNS_SELF       = 1u << 1u,
    DEQUE_FLAG_DYNAMIC_STORAGE = 1u << 2u,
    DEQUE_FLAG_ARENA_STORAGE   = 1u << 3u,
    DEQUE_FLAG_GROWABLE        = 1u << 4u,
} deque_flag_t;

typedef deque_status_t (*deque_copy_fn)(void *dst, const void *src, void *user);
typedef void (*deque_destroy_fn)(void *elem, void *user);
typedef deque_status_t (*deque_visit_fn)(const void *elem, size_t index, void *user);

typedef struct deque {
    alignas(max_align_t) unsigned char bytes[MEMKIT_DEQUE_OBJ_BYTES];
} deque_t;

typedef struct deque_config {
    size_t elem_size;
    size_t capacity;

    void *storage;
    size_t storage_bytes;

    arena_t *arena;

    deque_copy_fn copy_fn;
    deque_destroy_fn destroy_fn;
    void *user;

    unsigned flags;
} deque_config_t;

[[nodiscard]] deque_status_t deque_init(deque_t *deque, const deque_config_t *config);
[[nodiscard]] deque_status_t deque_create(
    deque_t **deque,
    size_t elem_size,
    size_t initial_capacity,
    arena_t *arena,
    unsigned flags
);
void deque_deinit(deque_t *deque);
void deque_destroy(deque_t *deque);

[[nodiscard]] size_t deque_size(const deque_t *deque);
[[nodiscard]] size_t deque_capacity(const deque_t *deque);
[[nodiscard]] bool deque_empty(const deque_t *deque);
[[nodiscard]] bool deque_full(const deque_t *deque);

void deque_clear(deque_t *deque);

[[nodiscard]] deque_status_t deque_reserve(deque_t *deque, size_t min_capacity);

[[nodiscard]] deque_status_t deque_push_front(deque_t *deque, const void *elem);
[[nodiscard]] deque_status_t deque_push_back(deque_t *deque, const void *elem);
[[nodiscard]] deque_status_t deque_pop_front(deque_t *deque, void *out_elem);
[[nodiscard]] deque_status_t deque_pop_back(deque_t *deque, void *out_elem);

[[nodiscard]] deque_status_t deque_peek_front(const deque_t *deque, void *out_elem);
[[nodiscard]] deque_status_t deque_peek_back(const deque_t *deque, void *out_elem);
[[nodiscard]] deque_status_t deque_peek_at(const deque_t *deque, size_t index, void *out_elem);

[[nodiscard]] void *deque_front(deque_t *deque);
[[nodiscard]] const void *deque_front_const(const deque_t *deque);
[[nodiscard]] void *deque_back(deque_t *deque);
[[nodiscard]] const void *deque_back_const(const deque_t *deque);

[[nodiscard]] deque_status_t deque_foreach(const deque_t *deque, deque_visit_fn visit, void *user);

[[nodiscard]] static inline bool deque_status_ok(deque_status_t status)
{
    return status == DEQUE_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* DEQUE_H */
