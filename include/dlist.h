#ifndef DLIST_H
#define DLIST_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "arena_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dlist_status {
    DLIST_OK = 0,
    DLIST_ERR_NULL,
    DLIST_ERR_INVALID,
    DLIST_ERR_NOT_FOUND,
    DLIST_ERR_EMPTY,
    DLIST_ERR_FULL,
    DLIST_ERR_OOM,
    DLIST_ERR_UNSUPPORTED,
} dlist_status_t;

typedef enum dlist_flag : unsigned {
    DLIST_FLAG_NONE            = 0u,
    DLIST_FLAG_OWNS_STORAGE    = 1u << 0u,
    DLIST_FLAG_OWNS_SELF       = 1u << 1u,
    DLIST_FLAG_DYNAMIC_STORAGE = 1u << 2u,
    DLIST_FLAG_ARENA_STORAGE   = 1u << 3u,
    DLIST_FLAG_FIXED_CAPACITY  = 1u << 4u,
} dlist_flag_t;

typedef dlist_status_t (*dlist_copy_fn)(void *dst, const void *src, void *user);
typedef void (*dlist_destroy_fn)(void *elem, void *user);
typedef dlist_status_t (*dlist_visit_fn)(const void *elem, size_t index, void *user);
typedef bool (*dlist_pred_fn)(const void *elem, const void *user);

typedef struct dlist_node {
    struct dlist_node *prev;
    struct dlist_node *next;
#ifdef __cplusplus
    uint8_t data[1];
#else
    uint8_t data[];
#endif
} dlist_node_t;

typedef struct dlist {
    alignas(max_align_t) unsigned char bytes[MEMKIT_DLIST_OBJ_BYTES];
} dlist_t;

typedef struct dlist_config {
    size_t elem_size;

    size_t node_capacity;
    void *node_pool;
    size_t node_pool_bytes;

    arena_t *arena;

    dlist_copy_fn copy_fn;
    dlist_destroy_fn destroy_fn;
    void *user;

    unsigned flags;
} dlist_config_t;

[[nodiscard]] size_t dlist_node_stride(size_t elem_size);

[[nodiscard]] dlist_status_t dlist_init(dlist_t *list, const dlist_config_t *config);
[[nodiscard]] dlist_status_t dlist_create(
    dlist_t **list,
    size_t elem_size,
    arena_t *arena,
    unsigned flags
);
void dlist_deinit(dlist_t *list);
void dlist_destroy(dlist_t *list);

[[nodiscard]] size_t dlist_size(const dlist_t *list);
[[nodiscard]] size_t dlist_capacity(const dlist_t *list);
[[nodiscard]] bool dlist_empty(const dlist_t *list);
[[nodiscard]] bool dlist_full(const dlist_t *list);

void dlist_clear(dlist_t *list);

[[nodiscard]] dlist_status_t dlist_push_front(dlist_t *list, const void *elem);
[[nodiscard]] dlist_status_t dlist_push_back(dlist_t *list, const void *elem);
[[nodiscard]] dlist_status_t dlist_pop_front(dlist_t *list, void *out_elem);
[[nodiscard]] dlist_status_t dlist_pop_back(dlist_t *list, void *out_elem);

[[nodiscard]] dlist_status_t dlist_peek_front(const dlist_t *list, void *out_elem);
[[nodiscard]] dlist_status_t dlist_peek_back(const dlist_t *list, void *out_elem);
[[nodiscard]] dlist_status_t dlist_peek_at(const dlist_t *list, size_t index, void *out_elem);

[[nodiscard]] dlist_status_t dlist_insert_at(dlist_t *list, size_t index, const void *elem);
[[nodiscard]] dlist_status_t dlist_remove_at(dlist_t *list, size_t index, void *out_elem);
[[nodiscard]] dlist_status_t dlist_remove_first(
    dlist_t *list,
    dlist_pred_fn pred,
    const void *pred_user,
    void *out_elem
);

[[nodiscard]] void *dlist_front(dlist_t *list);
[[nodiscard]] const void *dlist_front_const(const dlist_t *list);
[[nodiscard]] void *dlist_back(dlist_t *list);
[[nodiscard]] const void *dlist_back_const(const dlist_t *list);

[[nodiscard]] dlist_status_t dlist_foreach(const dlist_t *list, dlist_visit_fn visit, void *user);
[[nodiscard]] dlist_status_t dlist_foreach_reverse(
    const dlist_t *list,
    dlist_visit_fn visit,
    void *user
);

[[nodiscard]] static inline bool dlist_status_ok(dlist_status_t status)
{
    return status == DLIST_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* DLIST_H */
