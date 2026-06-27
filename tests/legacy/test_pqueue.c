#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "pqueue.h"

typedef struct {
    uint32_t id;
    int32_t priority;
} task_t;

static int task_compare(const void *a, const void *b, void *user)
{
    (void)user;
    const task_t *left = (const task_t *)a;
    const task_t *right = (const task_t *)b;

    if (left->priority < right->priority) {
        return -1;
    }
    if (left->priority > right->priority) {
        return 1;
    }
    return 0;
}

static pqueue_status_t task_collect_visit(const void *elem, size_t index, void *user)
{
    task_t *const out = (task_t *)user;
    out[index] = *(const task_t *)elem;
    return PQUEUE_OK;
}

static void test_pqueue_caller_owned(void)
{
    static uint8_t storage[sizeof(task_t) * 4u];

    pqueue_t pqueue;
    assert(pqueue_status_ok(pqueue_init(&pqueue, &(pqueue_config_t){
        .elem_size = sizeof(task_t),
        .capacity = 4u,
        .storage = storage,
        .storage_bytes = sizeof storage,
        .compare_fn = task_compare,
    })));

    const task_t items[] = {
        { .id = 0u, .priority = 5 },
        { .id = 1u, .priority = 1 },
        { .id = 2u, .priority = 3 },
        { .id = 3u, .priority = 2 },
    };

    for (size_t i = 0u; i < 4u; ++i) {
        assert(pqueue_status_ok(pqueue_push(&pqueue, &items[i])));
    }

    assert(pqueue_full(&pqueue));

    const task_t extra = { .id = 99u, .priority = 0 };
    assert(pqueue_push(&pqueue, &extra) == PQUEUE_ERR_FULL);

    task_t top = {0};
    assert(pqueue_status_ok(pqueue_peek(&pqueue, &top)));
    assert(top.priority == 1);

    const int32_t expected_order[] = { 1, 2, 3, 5 };
    for (size_t i = 0u; i < 4u; ++i) {
        task_t out = {0};
        assert(pqueue_status_ok(pqueue_pop(&pqueue, &out)));
        assert(out.priority == expected_order[i]);
    }

    assert(pqueue_empty(&pqueue));
    pqueue_deinit(&pqueue);
}

static void test_pqueue_arena(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    pqueue_t *pqueue = NULL;
    assert(pqueue_status_ok(pqueue_create(
        &pqueue,
        sizeof(task_t),
        task_compare,
        2u,
        &arena,
        PQUEUE_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 20u; ++i) {
        const task_t task = {
            .id = i,
            .priority = (int32_t)((i * 7u + 3u) % 20u),
        };
        assert(pqueue_status_ok(pqueue_push(pqueue, &task)));
    }

    assert(pqueue_size(pqueue) == 20u);

    task_t collected[20];
    assert(pqueue_status_ok(pqueue_foreach(pqueue, task_collect_visit, collected)));
    assert(pqueue_size(pqueue) == 20u);

    for (int32_t expected = 0; expected < 20; ++expected) {
        task_t out = {0};
        assert(pqueue_status_ok(pqueue_pop(pqueue, &out)));
        assert(out.priority == expected);
    }

    assert(pqueue_empty(pqueue));
    pqueue_destroy(pqueue);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_pqueue_dynamic(void)
{
    pqueue_t *pqueue = NULL;
    assert(pqueue_status_ok(pqueue_create(
        &pqueue,
        sizeof(task_t),
        task_compare,
        100u,
        NULL,
        PQUEUE_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 100u; ++i) {
        const task_t task = {
            .id = i,
            .priority = (int32_t)((100u - i) % 37u),
        };
        assert(pqueue_status_ok(pqueue_push(pqueue, &task)));
    }

    int32_t previous = INT32_MIN;
    while (!pqueue_empty(pqueue)) {
        task_t out = {0};
        assert(pqueue_status_ok(pqueue_pop(pqueue, &out)));
        assert(out.priority >= previous);
        previous = out.priority;
    }

    pqueue_destroy(pqueue);
}
#endif

int main(void)
{
    test_pqueue_caller_owned();
    test_pqueue_arena();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_pqueue_dynamic();
#endif

    puts("pqueue: ok");
    return 0;
}
