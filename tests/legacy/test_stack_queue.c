#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "queue.h"
#include "stack.h"

typedef struct {
    uint32_t id;
    int32_t value;
} item_t;

static stack_status_t stack_sum_visit(const void *elem, size_t index, void *user)
{
    (void)index;
    const item_t *item = (const item_t *)elem;
    int32_t *sum = (int32_t *)user;
    *sum += item->value;
    return STACK_OK;
}

static queue_status_t queue_collect_visit(const void *elem, size_t index, void *user)
{
    item_t *const out = (item_t *)user;
    out[index] = *(const item_t *)elem;
    return QUEUE_OK;
}

static void test_stack_caller_owned(void)
{
    static uint8_t storage[sizeof(item_t) * 4u];

    cstack_t stack;
    assert(stack_status_ok(stack_init(&stack, &(stack_config_t){
        .elem_size = sizeof(item_t),
        .capacity = 4u,
        .storage = storage,
        .storage_bytes = sizeof storage,
    })));

    for (uint32_t i = 0u; i < 4u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(stack_status_ok(stack_push(&stack, &item)));
    }

    assert(stack_full(&stack));

    const item_t extra = { .id = 99u, .value = 99 };
    assert(stack_push(&stack, &extra) == STACK_ERR_FULL);

    item_t top = {0};
    assert(stack_status_ok(stack_peek(&stack, &top)));
    assert(top.id == 3u);

    assert(stack_status_ok(stack_pop(&stack, &top)));
    assert(top.id == 3u);
    assert(stack_size(&stack) == 3u);

    stack_deinit(&stack);
}

static void test_stack_arena(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    cstack_t *stack = NULL;
    assert(stack_status_ok(stack_create(
        &stack,
        sizeof(item_t),
        2u,
        &arena,
        STACK_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 10u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(stack_status_ok(stack_push(stack, &item)));
    }

    int32_t sum = 0;
    assert(stack_status_ok(stack_foreach(stack, stack_sum_visit, &sum)));

    stack_destroy(stack);
    arena_deinit(&arena);
}

static void test_queue_caller_owned(void)
{
    static uint8_t storage[sizeof(item_t) * 4u];

    queue_t queue;
    assert(queue_status_ok(queue_init(&queue, &(queue_config_t){
        .elem_size = sizeof(item_t),
        .capacity = 4u,
        .storage = storage,
        .storage_bytes = sizeof storage,
    })));

    for (uint32_t i = 0u; i < 4u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)(i * 10) };
        assert(queue_status_ok(queue_push(&queue, &item)));
    }

    assert(queue_full(&queue));

    const item_t extra = { .id = 99u, .value = 99 };
    assert(queue_push(&queue, &extra) == QUEUE_ERR_FULL);

    item_t front = {0};
    assert(queue_status_ok(queue_peek_front(&queue, &front)));
    assert(front.id == 0u);

    assert(queue_status_ok(queue_pop(&queue, &front)));
    assert(front.id == 0u);
    assert(queue_size(&queue) == 3u);

    queue_deinit(&queue);
}

static void test_queue_arena(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    queue_t *queue = NULL;
    assert(queue_status_ok(queue_create(
        &queue,
        sizeof(item_t),
        2u,
        &arena,
        QUEUE_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 10u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(queue_status_ok(queue_push(queue, &item)));
    }

    assert(queue_size(queue) == 10u);

    item_t collected[10];
    assert(queue_status_ok(queue_foreach(queue, queue_collect_visit, collected)));
    for (uint32_t i = 0u; i < 10u; ++i) {
        assert(collected[i].id == i);
    }

    queue_destroy(queue);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_stack_dynamic(void)
{
    cstack_t *stack = NULL;
    assert(stack_status_ok(stack_create(
        &stack,
        sizeof(item_t),
        1u,
        NULL,
        STACK_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 50u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(stack_status_ok(stack_push(stack, &item)));
    }

    assert(stack_size(stack) == 50u);
    stack_destroy(stack);
}

static void test_queue_dynamic(void)
{
    queue_t *queue = NULL;
    assert(queue_status_ok(queue_create(
        &queue,
        sizeof(item_t),
        1u,
        NULL,
        QUEUE_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 50u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(queue_status_ok(queue_push(queue, &item)));
    }

    for (uint32_t i = 0u; i < 50u; ++i) {
        item_t out = {0};
        assert(queue_status_ok(queue_pop(queue, &out)));
        assert(out.id == i);
    }

    assert(queue_empty(queue));
    queue_destroy(queue);
}
#endif

int main(void)
{
    test_stack_caller_owned();
    test_stack_arena();
    test_queue_caller_owned();
    test_queue_arena();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_stack_dynamic();
    test_queue_dynamic();
#endif

    puts("stack/queue: ok");
    return 0;
}
