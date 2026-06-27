#ifndef PQUEUE_H
#define PQUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pqueue_status {
    PQUEUE_OK = 0,
    PQUEUE_ERR_NULL,
    PQUEUE_ERR_INVALID,
    PQUEUE_ERR_EMPTY,
    PQUEUE_ERR_FULL,
    PQUEUE_ERR_OOM,
    PQUEUE_ERR_UNSUPPORTED,
} pqueue_status_t;

typedef enum pqueue_flag : unsigned {
    PQUEUE_FLAG_NONE            = 0u,
    PQUEUE_FLAG_OWNS_STORAGE    = 1u << 0u,
    PQUEUE_FLAG_OWNS_SELF       = 1u << 1u,
    PQUEUE_FLAG_DYNAMIC_STORAGE = 1u << 2u,
    PQUEUE_FLAG_ARENA_STORAGE   = 1u << 3u,
    PQUEUE_FLAG_GROWABLE        = 1u << 4u,
} pqueue_flag_t;

typedef int (*pqueue_compare_fn)(const void *a, const void *b, void *user);
typedef pqueue_status_t (*pqueue_copy_fn)(void *dst, const void *src, void *user);
typedef void (*pqueue_destroy_fn)(void *elem, void *user);
typedef pqueue_status_t (*pqueue_visit_fn)(const void *elem, size_t index, void *user);

typedef struct pqueue {
    alignas(max_align_t) unsigned char bytes[MEMKIT_PQUEUE_OBJ_BYTES];
} pqueue_t;

typedef struct pqueue_config {
    size_t elem_size;
    size_t capacity;

    void *storage;
    size_t storage_bytes;

    arena_t *arena;

    pqueue_compare_fn compare_fn; /* required; <0 means first arg is higher priority */
    pqueue_copy_fn copy_fn;
    pqueue_destroy_fn destroy_fn;
    void *user;

    unsigned flags;
} pqueue_config_t;

[[nodiscard]] pqueue_status_t pqueue_init(pqueue_t *pqueue, const pqueue_config_t *config);
[[nodiscard]] pqueue_status_t pqueue_create(
    pqueue_t **pqueue,
    size_t elem_size,
    pqueue_compare_fn compare_fn,
    size_t initial_capacity,
    arena_t *arena,
    unsigned flags
);
void pqueue_deinit(pqueue_t *pqueue);
void pqueue_destroy(pqueue_t *pqueue);

[[nodiscard]] size_t pqueue_size(const pqueue_t *pqueue);
[[nodiscard]] size_t pqueue_capacity(const pqueue_t *pqueue);
[[nodiscard]] bool pqueue_empty(const pqueue_t *pqueue);
[[nodiscard]] bool pqueue_full(const pqueue_t *pqueue);

void pqueue_clear(pqueue_t *pqueue);

[[nodiscard]] pqueue_status_t pqueue_reserve(pqueue_t *pqueue, size_t min_capacity);
[[nodiscard]] pqueue_status_t pqueue_push(pqueue_t *pqueue, const void *elem);
[[nodiscard]] pqueue_status_t pqueue_pop(pqueue_t *pqueue, void *out_elem);
[[nodiscard]] pqueue_status_t pqueue_peek(const pqueue_t *pqueue, void *out_elem);

[[nodiscard]] void *pqueue_top(pqueue_t *pqueue);
[[nodiscard]] const void *pqueue_top_const(const pqueue_t *pqueue);

[[nodiscard]] pqueue_status_t pqueue_foreach(const pqueue_t *pqueue, pqueue_visit_fn visit, void *user);

[[nodiscard]] static inline bool pqueue_status_ok(pqueue_status_t status)
{
    return status == PQUEUE_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* PQUEUE_H */
