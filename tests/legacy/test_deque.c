#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "deque.h"

typedef struct {
    uint32_t id;
    int32_t value;
} item_t;

static deque_status_t deque_collect_visit(const void *elem, size_t index, void *user)
{
    item_t *const out = (item_t *)user;
    out[index] = *(const item_t *)elem;
    return DEQUE_OK;
}

static void test_deque_caller_owned(void)
{
    static uint8_t storage[sizeof(item_t) * 4u];

    deque_t deque;
    assert(deque_status_ok(deque_init(&deque, &(deque_config_t){
        .elem_size = sizeof(item_t),
        .capacity = 4u,
        .storage = storage,
        .storage_bytes = sizeof storage,
    })));

    const item_t back = { .id = 1u, .value = 10 };
    const item_t front = { .id = 2u, .value = 20 };

    assert(deque_status_ok(deque_push_back(&deque, &back)));
    assert(deque_status_ok(deque_push_front(&deque, &front)));
    assert(deque_size(&deque) == 2u);

    item_t peek = {0};
    assert(deque_status_ok(deque_peek_front(&deque, &peek)));
    assert(peek.id == 2u);
    assert(deque_status_ok(deque_peek_back(&deque, &peek)));
    assert(peek.id == 1u);

    for (uint32_t i = 0u; i < 2u; ++i) {
        const item_t item = { .id = 10u + i, .value = (int32_t)i };
        assert(deque_status_ok(deque_push_back(&deque, &item)));
    }

    assert(deque_full(&deque));
    {
        const item_t extra = { .id = 99u, .value = 99 };
        assert(deque_push_front(&deque, &extra) == DEQUE_ERR_FULL);
    }

    assert(deque_status_ok(deque_pop_front(&deque, &peek)));
    assert(peek.id == 2u);
    assert(deque_status_ok(deque_pop_back(&deque, &peek)));
    assert(peek.id == 11u);
    assert(deque_size(&deque) == 2u);

    deque_clear(&deque);
    assert(deque_empty(&deque));

    deque_deinit(&deque);
}

static void test_deque_arena(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    deque_t *deque = NULL;
    assert(deque_status_ok(deque_create(
        &deque,
        sizeof(item_t),
        2u,
        &arena,
        DEQUE_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 10u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        if ((i % 2u) == 0u) {
            assert(deque_status_ok(deque_push_back(deque, &item)));
        } else {
            assert(deque_status_ok(deque_push_front(deque, &item)));
        }
    }

    assert(deque_size(deque) == 10u);

    item_t collected[10];
    assert(deque_status_ok(deque_foreach(deque, deque_collect_visit, collected)));

    item_t front = {0};
    item_t back = {0};
    assert(deque_status_ok(deque_peek_front(deque, &front)));
    assert(deque_status_ok(deque_peek_back(deque, &back)));
    assert(front.id == collected[0].id);
    assert(back.id == collected[9].id);

    deque_destroy(deque);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_deque_dynamic(void)
{
    deque_t *deque = NULL;
    assert(deque_status_ok(deque_create(
        &deque,
        sizeof(item_t),
        1u,
        NULL,
        DEQUE_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 100u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(deque_status_ok(deque_push_back(deque, &item)));
    }

    for (uint32_t i = 0u; i < 50u; ++i) {
        item_t out = {0};
        assert(deque_status_ok(deque_pop_front(deque, &out)));
        assert(out.id == i);
    }

    for (uint32_t expected = 99u; expected >= 50u; --expected) {
        item_t out = {0};
        assert(deque_status_ok(deque_pop_back(deque, &out)));
        assert(out.id == expected);
    }

    assert(deque_empty(deque));
    deque_destroy(deque);
}
#endif

int main(void)
{
    test_deque_caller_owned();
    test_deque_arena();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_deque_dynamic();
#endif

    puts("deque: ok");
    return 0;
}
