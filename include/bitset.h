#ifndef BITSET_H
#define BITSET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bitset_status {
    BITSET_OK = 0,
    BITSET_ERR_NULL,
    BITSET_ERR_INVALID,
    BITSET_ERR_NOT_FOUND,
    BITSET_ERR_EMPTY,
    BITSET_ERR_FULL,
    BITSET_ERR_OOM,
    BITSET_ERR_UNSUPPORTED,
} bitset_status_t;

typedef enum bitset_flag : unsigned {
    BITSET_FLAG_NONE            = 0u,
    BITSET_FLAG_OWNS_STORAGE    = 1u << 0u, /* bitset frees bit storage on deinit */
    BITSET_FLAG_OWNS_SELF       = 1u << 1u,
    BITSET_FLAG_DYNAMIC_STORAGE = 1u << 2u,
    BITSET_FLAG_ARENA_STORAGE   = 1u << 3u,
    BITSET_FLAG_FIXED_CAPACITY  = 1u << 4u, /* fixed bit count; no grow */
} bitset_flag_t;

typedef bitset_status_t (*bitset_visit_fn)(size_t index, void *user);

typedef struct bitset {
    alignas(max_align_t) unsigned char bytes[MEMKIT_BITSET_OBJ_BYTES];
} bitset_t;

typedef struct bitset_config {
    size_t capacity;      /* number of bits */

    void *storage;        /* caller-owned bytes; use bitset_storage_bytes(capacity) */
    size_t storage_bytes;

    arena_t *arena;
    void *user;

    unsigned flags;
} bitset_config_t;

[[nodiscard]] size_t bitset_storage_bytes(size_t capacity);
[[nodiscard]] uint8_t bitset_tail_mask(size_t capacity);

[[nodiscard]] bitset_status_t bitset_init(bitset_t *bitset, const bitset_config_t *config);
[[nodiscard]] bitset_status_t bitset_create(
    bitset_t **bitset,
    size_t capacity,
    arena_t *arena,
    unsigned flags
);
void bitset_deinit(bitset_t *bitset);
void bitset_destroy(bitset_t *bitset);

[[nodiscard]] size_t bitset_capacity(const bitset_t *bitset);
[[nodiscard]] size_t bitset_size(const bitset_t *bitset);
[[nodiscard]] bool bitset_empty(const bitset_t *bitset);
[[nodiscard]] bool bitset_full(const bitset_t *bitset);

void bitset_clear(bitset_t *bitset);
[[nodiscard]] bitset_status_t bitset_set_all(bitset_t *bitset);

[[nodiscard]] bool bitset_test(const bitset_t *bitset, size_t index);
[[nodiscard]] bitset_status_t bitset_set(bitset_t *bitset, size_t index);
[[nodiscard]] bitset_status_t bitset_reset(bitset_t *bitset, size_t index);
[[nodiscard]] bitset_status_t bitset_toggle(bitset_t *bitset, size_t index);
[[nodiscard]] bitset_status_t bitset_assign(bitset_t *bitset, bool value, size_t index);

[[nodiscard]] bitset_status_t bitset_find_first_set(
    const bitset_t *bitset,
    size_t start_index,
    size_t *out_index
);
[[nodiscard]] bitset_status_t bitset_find_first_clear(
    const bitset_t *bitset,
    size_t start_index,
    size_t *out_index
);

[[nodiscard]] bitset_status_t bitset_copy(bitset_t *dst, const bitset_t *src);
[[nodiscard]] bool bitset_equal(const bitset_t *left, const bitset_t *right);

[[nodiscard]] bitset_status_t bitset_union_with(bitset_t *bitset, const bitset_t *other);
[[nodiscard]] bitset_status_t bitset_intersect_with(bitset_t *bitset, const bitset_t *other);
[[nodiscard]] bitset_status_t bitset_xor_with(bitset_t *bitset, const bitset_t *other);
[[nodiscard]] bitset_status_t bitset_complement(bitset_t *bitset);

[[nodiscard]] bitset_status_t bitset_load_bytes(
    bitset_t *bitset,
    const void *bytes,
    size_t bytes_len
);
[[nodiscard]] bitset_status_t bitset_store_bytes(
    const bitset_t *bitset,
    void *bytes,
    size_t bytes_len
);

[[nodiscard]] uint8_t *bitset_data(bitset_t *bitset);
[[nodiscard]] const uint8_t *bitset_data_const(const bitset_t *bitset);
[[nodiscard]] size_t bitset_data_bytes(const bitset_t *bitset);

[[nodiscard]] bitset_status_t bitset_foreach(
    const bitset_t *bitset,
    bitset_visit_fn visit,
    void *user
);

[[nodiscard]] static inline bool bitset_status_ok(bitset_status_t status)
{
    return status == BITSET_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* BITSET_H */
