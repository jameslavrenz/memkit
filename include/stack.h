#ifndef STACK_H
#define STACK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "arena.h"
#include "memkit_config.h"
#include "memkit_object_sizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum stack_status {
    STACK_OK = 0,
    STACK_ERR_NULL,
    STACK_ERR_INVALID,
    STACK_ERR_EMPTY,
    STACK_ERR_FULL,
    STACK_ERR_OOM,
    STACK_ERR_UNSUPPORTED,
} stack_status_t;

typedef enum stack_flag : unsigned {
    STACK_FLAG_NONE            = 0u,
    STACK_FLAG_OWNS_STORAGE    = 1u << 0u,
    STACK_FLAG_OWNS_SELF       = 1u << 1u,
    STACK_FLAG_DYNAMIC_STORAGE = 1u << 2u,
    STACK_FLAG_ARENA_STORAGE   = 1u << 3u,
    STACK_FLAG_GROWABLE        = 1u << 4u,
} stack_flag_t;

typedef stack_status_t (*stack_copy_fn)(void *dst, const void *src, void *user);
typedef void (*stack_destroy_fn)(void *elem, void *user);
typedef stack_status_t (*stack_visit_fn)(const void *elem, size_t index, void *user);

typedef struct stack {
    alignas(max_align_t) unsigned char bytes[MEMKIT_STACK_OBJ_BYTES];
} cstack_t;

typedef struct stack_config {
    size_t elem_size;
    size_t capacity;

    void *storage;
    size_t storage_bytes;

    arena_t *arena;

    stack_copy_fn copy_fn;
    stack_destroy_fn destroy_fn;
    void *user;

    unsigned flags;
} stack_config_t;

[[nodiscard]] stack_status_t stack_init(cstack_t *stack, const stack_config_t *config);
[[nodiscard]] stack_status_t stack_create(
    cstack_t **stack,
    size_t elem_size,
    size_t initial_capacity,
    arena_t *arena,
    unsigned flags
);
void stack_deinit(cstack_t *stack);
void stack_destroy(cstack_t *stack);

[[nodiscard]] size_t stack_size(const cstack_t *stack);
[[nodiscard]] size_t stack_capacity(const cstack_t *stack);
[[nodiscard]] bool stack_empty(const cstack_t *stack);
[[nodiscard]] bool stack_full(const cstack_t *stack);

void stack_clear(cstack_t *stack);

[[nodiscard]] stack_status_t stack_reserve(cstack_t *stack, size_t min_capacity);
[[nodiscard]] stack_status_t stack_push(cstack_t *stack, const void *elem);
[[nodiscard]] stack_status_t stack_pop(cstack_t *stack, void *out_elem);
[[nodiscard]] stack_status_t stack_peek(const cstack_t *stack, void *out_elem);

[[nodiscard]] void *stack_top(cstack_t *stack);
[[nodiscard]] const void *stack_top_const(const cstack_t *stack);

[[nodiscard]] stack_status_t stack_foreach(const cstack_t *stack, stack_visit_fn visit, void *user);

[[nodiscard]] static inline bool stack_status_ok(stack_status_t status)
{
    return status == STACK_OK;
}


#ifdef __cplusplus
}
#endif
#endif /* STACK_H */
