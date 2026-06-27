#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "deque.h"

typedef struct item {
    uint32_t id;
    int32_t value;
} item_t;

static void test_caller_owned(void)
{
    static uint8_t storage[sizeof(item_t) * 4u];

    deque_t deque;
    assert(deque_status_ok(deque_init(&deque, &(deque_config_t){
        .elem_size     = sizeof(item_t),
        .capacity      = 4u,
        .storage       = storage,
        .storage_bytes = sizeof storage,
    })));

    const item_t back = {.id = 1u, .value = 10};
    const item_t front = {.id = 2u, .value = 20};
    assert(deque_status_ok(deque_push_back(&deque, &back)));
    assert(deque_status_ok(deque_push_front(&deque, &front)));
    assert(deque_size(&deque) == 2u);

    item_t peek = {0};
    assert(deque_status_ok(deque_peek_front(&deque, &peek)));
    assert(peek.id == 2u);
    assert(deque_status_ok(deque_peek_back(&deque, &peek)));
    assert(peek.id == 1u);

    for (uint32_t i = 0u; i < 2u; ++i) {
        const item_t value = {.id = 10u + i, .value = (int32_t)i};
        assert(deque_status_ok(deque_push_back(&deque, &value)));
    }

    assert(deque_full(&deque));
    assert(deque_status_ok(deque_pop_front(&deque, &peek)));
    assert(peek.id == 2u);
    assert(deque_status_ok(deque_pop_back(&deque, &peek)));
    assert(peek.id == 11u);

    deque_clear(&deque);
    assert(deque_empty(&deque));
    deque_deinit(&deque);
}

static void test_growable(void)
{
    deque_t deque;
    assert(deque_status_ok(deque_init(&deque, &(deque_config_t){
        .elem_size = sizeof(item_t),
        .capacity  = 2u,
        .flags     = DEQUE_FLAG_DYNAMIC_STORAGE | DEQUE_FLAG_GROWABLE,
    })));

    for (uint32_t i = 0u; i < 10u; ++i) {
        const item_t value = {.id = i, .value = (int32_t)i};
        if ((i % 2u) == 0u) {
            assert(deque_status_ok(deque_push_back(&deque, &value)));
        } else {
            assert(deque_status_ok(deque_push_front(&deque, &value)));
        }
    }

    assert(deque_size(&deque) == 10u);
    deque_deinit(&deque);
}

int main(void)
{
    test_caller_owned();
    test_growable();
    printf("deque_c: ok\n");
    return 0;
}
