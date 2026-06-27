#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum queue_status {
    QUEUE_OK = 0,
    QUEUE_ERR_NULL,
    QUEUE_ERR_INVALID,
    QUEUE_ERR_EMPTY,
    QUEUE_ERR_FULL,
    QUEUE_ERR_OOM,
    QUEUE_ERR_UNSUPPORTED,
} queue_status_t;

typedef enum queue_flag : unsigned {
    QUEUE_FLAG_NONE            = 0u,
    QUEUE_FLAG_OWNS_STORAGE    = 1u << 0u, /* queue frees element storage on deinit */
    QUEUE_FLAG_OWNS_SELF       = 1u << 1u, /* queue object was heap/arena allocated */
    QUEUE_FLAG_DYNAMIC_STORAGE = 1u << 2u, /* element storage from heap (MPU only) */
    QUEUE_FLAG_ARENA_STORAGE   = 1u << 3u, /* element storage from bump arena */
    QUEUE_FLAG_GROWABLE        = 1u << 4u, /* double capacity when full (MPU) */
} queue_flag_t;

typedef queue_status_t (*queue_copy_fn)(void *dst, const void *src, void *user);
typedef void (*queue_destroy_fn)(void *elem, void *user);
typedef queue_status_t (*queue_visit_fn)(const void *elem, size_t index, void *user);

typedef struct queue {
    alignas(max_align_t) unsigned char bytes[MEMKIT_QUEUE_OBJ_BYTES];
} queue_t;

typedef struct queue_config {
    size_t elem_size;     /* sizeof one element in bytes */
    size_t capacity;      /* max elements in storage */

    void *storage;        /* caller-owned buffer; NULL if create/arena owns storage */
    size_t storage_bytes; /* byte size of storage; >= elem_size * capacity */

    arena_t *arena;

    queue_copy_fn copy_fn;
    queue_destroy_fn destroy_fn;
    void *user;

    unsigned flags;       /* QUEUE_FLAG_* */
} queue_config_t;

[[nodiscard]] queue_status_t queue_init(queue_t *queue, const queue_config_t *config);
[[nodiscard]] queue_status_t queue_create(
    queue_t **queue,
    size_t elem_size,
    size_t initial_capacity,
    arena_t *arena,
    unsigned flags
);
void queue_deinit(queue_t *queue);
void queue_destroy(queue_t *queue);

[[nodiscard]] size_t queue_size(const queue_t *queue);
[[nodiscard]] size_t queue_capacity(const queue_t *queue);
[[nodiscard]] bool queue_empty(const queue_t *queue);
[[nodiscard]] bool queue_full(const queue_t *queue);

void queue_clear(queue_t *queue);

[[nodiscard]] queue_status_t queue_push(queue_t *queue, const void *elem);
[[nodiscard]] queue_status_t queue_pop(queue_t *queue, void *out_elem);

[[nodiscard]] queue_status_t queue_peek_front(const queue_t *queue, void *out_elem);
[[nodiscard]] queue_status_t queue_peek_back(const queue_t *queue, void *out_elem);
[[nodiscard]] queue_status_t queue_peek_at(const queue_t *queue, size_t index, void *out_elem);

[[nodiscard]] queue_status_t queue_foreach(const queue_t *queue, queue_visit_fn visit, void *user);

[[nodiscard]] size_t queue_readable_contiguous(const queue_t *queue, const void **out_ptr);
[[nodiscard]] size_t queue_writable_contiguous(queue_t *queue, void **out_ptr);
void queue_commit_read(queue_t *queue, size_t elem_count);
void queue_commit_write(queue_t *queue, size_t elem_count);

[[nodiscard]] static inline bool queue_status_ok(queue_status_t status)
{
    return status == QUEUE_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* QUEUE_H */
