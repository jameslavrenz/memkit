#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "stack.h"

typedef struct item {
    uint32_t id;
    int32_t value;
} item_t;

static void test_caller_owned(void)
{
    static uint8_t storage[sizeof(item_t) * 4u];

    cstack_t stack;
    assert(stack_status_ok(stack_init(&stack, &(stack_config_t){
        .elem_size     = sizeof(item_t),
        .capacity      = 4u,
        .storage       = storage,
        .storage_bytes = sizeof storage,
    })));

    for (uint32_t i = 0u; i < 4u; ++i) {
        const item_t item = {.id = i, .value = (int32_t)i};
        assert(stack_status_ok(stack_push(&stack, &item)));
    }

    assert(stack_full(&stack));

    item_t top = {0};
    assert(stack_status_ok(stack_peek(&stack, &top)));
    assert(top.id == 3u);

    assert(stack_status_ok(stack_pop(&stack, &top)));
    assert(top.id == 3u);
    assert(stack_size(&stack) == 3u);

    stack_deinit(&stack);
}

static void test_arena_growable(void)
{
    static uint8_t arena_backing[4096u];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing       = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    cstack_t stack;
    assert(stack_status_ok(stack_init(&stack, &(stack_config_t){
        .elem_size = sizeof(item_t),
        .capacity  = 2u,
        .arena     = &arena,
        .flags     = STACK_FLAG_ARENA_STORAGE | STACK_FLAG_GROWABLE,
    })));

    for (uint32_t i = 0u; i < 10u; ++i) {
        const item_t item = {.id = i, .value = (int32_t)i};
        assert(stack_status_ok(stack_push(&stack, &item)));
    }

    assert(stack_size(&stack) == 10u);
    stack_deinit(&stack);
    arena_deinit(&arena);
}

int main(void)
{
    test_caller_owned();
    test_arena_growable();
    printf("stack_c: ok\n");
    return 0;
}
