#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum list_status {
    LIST_OK = 0,
    LIST_ERR_NULL,
    LIST_ERR_INVALID,
    LIST_ERR_NOT_FOUND,
    LIST_ERR_EMPTY,
    LIST_ERR_FULL,
    LIST_ERR_OOM,
    LIST_ERR_UNSUPPORTED,
} list_status_t;

typedef enum list_flag : unsigned {
    LIST_FLAG_NONE            = 0u,
    LIST_FLAG_OWNS_STORAGE    = 1u << 0u, /* list frees node pool on deinit */
    LIST_FLAG_OWNS_SELF       = 1u << 1u,
    LIST_FLAG_DYNAMIC_STORAGE = 1u << 2u,
    LIST_FLAG_ARENA_STORAGE   = 1u << 3u,
    LIST_FLAG_FIXED_CAPACITY  = 1u << 4u, /* push fails when node pool exhausted */
} list_flag_t;

typedef list_status_t (*list_copy_fn)(void *dst, const void *src, void *user);
typedef void (*list_destroy_fn)(void *elem, void *user);
typedef list_status_t (*list_visit_fn)(const void *elem, size_t index, void *user);
typedef bool (*list_pred_fn)(const void *elem, const void *user);

typedef struct list_node {
    struct list_node *next;
#ifdef __cplusplus
    uint8_t data[1];
#else
    uint8_t data[];
#endif
} list_node_t;

typedef struct list {
    alignas(max_align_t) unsigned char bytes[MEMKIT_LIST_OBJ_BYTES];
} list_t;

typedef struct list_config {
    size_t elem_size;       /* payload bytes per node */

    size_t node_capacity;   /* max nodes in pool */
    void *node_pool;        /* byte slab; size >= node_capacity * list_node_stride(elem_size) */
    size_t node_pool_bytes;

    arena_t *arena;

    list_copy_fn copy_fn;
    list_destroy_fn destroy_fn;
    void *user;

    unsigned flags;
} list_config_t;

[[nodiscard]] size_t list_node_stride(size_t elem_size);

[[nodiscard]] list_status_t list_init(list_t *list, const list_config_t *config);
[[nodiscard]] list_status_t list_create(
    list_t **list,
    size_t elem_size,
    arena_t *arena,
    unsigned flags
);
void list_deinit(list_t *list);
void list_destroy(list_t *list);

[[nodiscard]] size_t list_size(const list_t *list);
[[nodiscard]] size_t list_capacity(const list_t *list);
[[nodiscard]] bool list_empty(const list_t *list);
[[nodiscard]] bool list_full(const list_t *list);

void list_clear(list_t *list);

[[nodiscard]] list_status_t list_push_front(list_t *list, const void *elem);
[[nodiscard]] list_status_t list_push_back(list_t *list, const void *elem);
[[nodiscard]] list_status_t list_pop_front(list_t *list, void *out_elem);
[[nodiscard]] list_status_t list_pop_back(list_t *list, void *out_elem);

[[nodiscard]] list_status_t list_peek_front(const list_t *list, void *out_elem);
[[nodiscard]] list_status_t list_peek_back(const list_t *list, void *out_elem);
[[nodiscard]] list_status_t list_peek_at(const list_t *list, size_t index, void *out_elem);

[[nodiscard]] list_status_t list_insert_at(list_t *list, size_t index, const void *elem);
[[nodiscard]] list_status_t list_remove_at(list_t *list, size_t index, void *out_elem);
[[nodiscard]] list_status_t list_remove_first(
    list_t *list,
    list_pred_fn pred,
    const void *pred_user,
    void *out_elem
);

[[nodiscard]] void *list_front(list_t *list);
[[nodiscard]] const void *list_front_const(const list_t *list);

[[nodiscard]] list_status_t list_foreach(const list_t *list, list_visit_fn visit, void *user);

[[nodiscard]] static inline bool list_status_ok(list_status_t status)
{
    return status == LIST_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* LIST_H */
