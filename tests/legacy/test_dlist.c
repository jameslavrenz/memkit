#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "dlist.h"

typedef struct {
    uint32_t id;
    int32_t value;
} item_t;

static dlist_status_t sum_values(const void *elem, size_t index, void *user)
{
    (void)index;
    const item_t *item = (const item_t *)elem;
    int32_t *sum = (int32_t *)user;
    *sum += item->value;
    return DLIST_OK;
}

static bool item_id_equals(const void *elem, const void *user)
{
    const item_t *item = (const item_t *)elem;
    const uint32_t *target = (const uint32_t *)user;
    return item->id == *target;
}

static void test_caller_owned_node_pool(void)
{
    enum { CAPACITY = 4u };
    const size_t stride = dlist_node_stride(sizeof(item_t));
    static uint8_t pool[CAPACITY * 64u];

    dlist_t list;
    assert(dlist_status_ok(dlist_init(&list, &(dlist_config_t){
        .elem_size = sizeof(item_t),
        .node_capacity = CAPACITY,
        .node_pool = pool,
        .node_pool_bytes = CAPACITY * stride,
    })));

    for (uint32_t i = 0u; i < CAPACITY; ++i) {
        const item_t item = { .id = i, .value = (int32_t)(i * 10) };
        assert(dlist_status_ok(dlist_push_back(&list, &item)));
    }

    assert(dlist_full(&list));

    const item_t extra = { .id = 99u, .value = 99 };
    assert(dlist_push_back(&list, &extra) == DLIST_ERR_FULL);

    item_t back = {0};
    assert(dlist_status_ok(dlist_peek_back(&list, &back)));
    assert(back.id == 3u);

    assert(dlist_status_ok(dlist_pop_back(&list, NULL)));
    assert(dlist_size(&list) == 3u);

    dlist_clear(&list);
    dlist_deinit(&list);
}

static dlist_status_t count_visit(const void *elem, size_t index, void *user)
{
    (void)elem;
    (void)index;
    size_t *count = (size_t *)user;
    (*count)++;
    return DLIST_OK;
}

static void test_arena_dynamic_nodes(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    dlist_t list;
    assert(dlist_status_ok(dlist_init(&list, &(dlist_config_t){
        .elem_size = sizeof(item_t),
        .arena = &arena,
    })));

    for (uint32_t i = 0u; i < 10u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(dlist_status_ok(dlist_push_front(&list, &item)));
    }

    item_t front = {0};
    assert(dlist_status_ok(dlist_peek_front(&list, &front)));
    assert(front.id == 9u);

    const item_t middle = { .id = 50u, .value = 500 };
    assert(dlist_status_ok(dlist_insert_at(&list, 5u, &middle)));

    item_t at = {0};
    assert(dlist_status_ok(dlist_peek_at(&list, 5u, &at)));
    assert(at.id == 50u);

    int32_t sum = 0;
    assert(dlist_status_ok(dlist_foreach(&list, sum_values, &sum)));

    const uint32_t remove_id = 50u;
    assert(dlist_status_ok(dlist_remove_first(&list, item_id_equals, &remove_id, NULL)));
    assert(dlist_size(&list) == 10u);

    size_t reverse_count = 0u;
    assert(dlist_status_ok(dlist_foreach_reverse(&list, count_visit, &reverse_count)));
    assert(reverse_count == dlist_size(&list));

    assert(dlist_back_const(&list) != NULL);
    assert(dlist_status_ok(dlist_pop_front(&list, NULL)));
    assert(dlist_status_ok(dlist_pop_back(&list, NULL)));
    assert(dlist_size(&list) == 8u);

    dlist_deinit(&list);
    arena_deinit(&arena);
}

static void test_dlist_create(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    dlist_t *list = NULL;
    assert(dlist_status_ok(dlist_create(&list, sizeof(item_t), &arena, DLIST_FLAG_NONE)));

    const item_t item = { .id = 1u, .value = 42 };
    assert(dlist_status_ok(dlist_push_back(list, &item)));
    assert(dlist_size(list) == 1u);

    dlist_destroy(list);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_dynamic_dlist(void)
{
    dlist_t *list = NULL;
    assert(dlist_status_ok(dlist_create(&list, sizeof(item_t), NULL, DLIST_FLAG_NONE)));

    for (uint32_t i = 0u; i < 100u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(dlist_status_ok(dlist_push_back(list, &item)));
    }

    item_t popped = {0};
    assert(dlist_status_ok(dlist_pop_back(list, &popped)));
    assert(popped.id == 99u);

    assert(dlist_status_ok(dlist_pop_front(list, &popped)));
    assert(popped.id == 0u);

    assert(dlist_size(list) == 98u);
    dlist_destroy(list);
}
#endif

int main(void)
{
    test_caller_owned_node_pool();
    test_arena_dynamic_nodes();
    test_dlist_create();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_dynamic_dlist();
#endif

    puts("dlist: ok");
    return 0;
}
