#ifndef BTREE_H
#define BTREE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum btree_status {
    BTREE_OK = 0,
    BTREE_ERR_NULL,
    BTREE_ERR_INVALID,
    BTREE_ERR_NOT_FOUND,
    BTREE_ERR_EMPTY,
    BTREE_ERR_FULL,
    BTREE_ERR_OOM,
    BTREE_ERR_UNSUPPORTED,
} btree_status_t;

typedef enum btree_flag : unsigned {
    BTREE_FLAG_NONE            = 0u,
    BTREE_FLAG_OWNS_STORAGE    = 1u << 0u, /* btree frees node-pool backing on deinit */
    BTREE_FLAG_OWNS_SELF       = 1u << 1u, /* btree struct was heap/arena allocated */
    BTREE_FLAG_DYNAMIC_STORAGE = 1u << 2u, /* nodes/pool came from heap (if enabled) */
    BTREE_FLAG_ARENA_STORAGE   = 1u << 3u, /* nodes/pool came from an arena */
    BTREE_FLAG_FIXED_CAPACITY  = 1u << 4u, /* caller/owned node pool; insert fails when exhausted */
} btree_flag_t;

typedef enum btree_traversal {
    BTREE_TRAVERSAL_INORDER = 0,
    BTREE_TRAVERSAL_PREORDER = 1,
    BTREE_TRAVERSAL_POSTORDER = 2,
} btree_traversal_t;

typedef int (*btree_compare_fn)(const void *a, const void *b, void *user);
typedef btree_status_t (*btree_copy_fn)(void *dst, const void *src, void *user);
typedef void (*btree_destroy_fn)(void *elem, void *user);
typedef btree_status_t (*btree_visit_fn)(const void *elem, void *user);

typedef struct btree_node {
    struct btree_node *left;
    struct btree_node *right;
#ifdef __cplusplus
    uint8_t data[1];
#else
    uint8_t data[];
#endif
} btree_node_t;

typedef struct btree {
    alignas(max_align_t) unsigned char bytes[MEMKIT_BTREE_OBJ_BYTES];
} btree_t;

typedef struct btree_config {
    size_t elem_size;

    size_t node_capacity;
    void *node_pool;
    size_t node_pool_bytes;

    arena_t *arena;

    btree_compare_fn compare_fn;
    btree_copy_fn copy_fn;
    btree_destroy_fn destroy_fn;
    void *user;

    unsigned flags;
} btree_config_t;

[[nodiscard]] int btree_compare_bytes(const void *a, const void *b, size_t elem_size, void *user);

[[nodiscard]] size_t btree_node_stride(size_t elem_size);

[[nodiscard]] btree_status_t btree_init(btree_t *tree, const btree_config_t *config);
[[nodiscard]] btree_status_t btree_create(
    btree_t **tree,
    size_t elem_size,
    btree_compare_fn compare_fn,
    arena_t *arena,
    unsigned flags
);
void btree_deinit(btree_t *tree);
void btree_destroy(btree_t *tree);

[[nodiscard]] size_t btree_size(const btree_t *tree);
[[nodiscard]] size_t btree_capacity(const btree_t *tree);
[[nodiscard]] bool btree_empty(const btree_t *tree);
[[nodiscard]] bool btree_full(const btree_t *tree);

void btree_clear(btree_t *tree);

[[nodiscard]] btree_status_t btree_insert(btree_t *tree, const void *elem);
[[nodiscard]] btree_status_t btree_get(const btree_t *tree, const void *key, void *out_elem);
[[nodiscard]] bool btree_contains(const btree_t *tree, const void *key);
[[nodiscard]] btree_status_t btree_remove(btree_t *tree, const void *key);

[[nodiscard]] btree_status_t btree_peek_min(const btree_t *tree, void *out_elem);
[[nodiscard]] btree_status_t btree_peek_max(const btree_t *tree, void *out_elem);

[[nodiscard]] btree_status_t btree_foreach(
    const btree_t *tree,
    btree_traversal_t order,
    btree_visit_fn visit,
    void *user
);

[[nodiscard]] static inline bool btree_status_ok(btree_status_t status)
{
    return status == BTREE_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* BTREE_H */
