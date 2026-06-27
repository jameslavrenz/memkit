#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "queue.h"

typedef struct item {
    uint32_t id;
    int32_t value;
} item_t;

static void test_caller_owned(void)
{
    static uint8_t storage[sizeof(item_t) * 4u];

    queue_t queue;
    assert(queue_status_ok(queue_init(&queue, &(queue_config_t){
        .elem_size     = sizeof(item_t),
        .capacity      = 4u,
        .storage       = storage,
        .storage_bytes = sizeof storage,
    })));

    for (uint32_t i = 0u; i < 4u; ++i) {
        const item_t item = {.id = i, .value = (int32_t)(i * 10)};
        assert(queue_status_ok(queue_push(&queue, &item)));
    }

    assert(queue_full(&queue));

    item_t extra = {.id = 99u, .value = 99};
    assert(queue_push(&queue, &extra) == QUEUE_ERR_FULL);

    item_t front = {0};
    assert(queue_status_ok(queue_peek_front(&queue, &front)));
    assert(front.id == 0u);

    assert(queue_status_ok(queue_pop(&queue, &front)));
    assert(front.id == 0u);
    assert(queue_size(&queue) == 3u);

    queue_deinit(&queue);
}

static void test_arena_create(void)
{
    static uint8_t arena_backing[2048u];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing       = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    queue_t *queue = NULL;
    assert(queue_status_ok(queue_create(
        &queue,
        sizeof(item_t),
        4u,
        &arena,
        0u
    )));

    for (uint32_t i = 0u; i < 4u; ++i) {
        const item_t item = {.id = i, .value = (int32_t)i};
        assert(queue_status_ok(queue_push(queue, &item)));
    }

    for (uint32_t i = 0u; i < 4u; ++i) {
        item_t out = {0};
        assert(queue_status_ok(queue_pop(queue, &out)));
        assert(out.id == i);
    }

    queue_destroy(queue);
    arena_deinit(&arena);
}

int main(void)
{
    test_caller_owned();
    test_arena_create();
    printf("queue_c: ok\n");
    return 0;
}
