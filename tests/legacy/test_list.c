#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "list.h"

typedef struct {
    uint32_t id;
    int32_t value;
} item_t;

static list_status_t sum_values(const void *elem, size_t index, void *user)
{
    (void)index;
    const item_t *item = (const item_t *)elem;
    int32_t *sum = (int32_t *)user;
    *sum += item->value;
    return LIST_OK;
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
    const size_t stride = list_node_stride(sizeof(item_t));
    static uint8_t pool[CAPACITY * 32u];

    list_t list;
    assert(list_status_ok(list_init(&list, &(list_config_t){
        .elem_size = sizeof(item_t),
        .node_capacity = CAPACITY,
        .node_pool = pool,
        .node_pool_bytes = CAPACITY * stride,
    })));

    assert(list_empty(&list));
    assert(list_capacity(&list) == CAPACITY);

    for (uint32_t i = 0u; i < CAPACITY; ++i) {
        const item_t item = { .id = i, .value = (int32_t)(i * 10) };
        assert(list_status_ok(list_push_back(&list, &item)));
    }

    assert(list_full(&list));
    {
        const item_t extra = { .id = 99u, .value = 99 };
        assert(list_push_back(&list, &extra) == LIST_ERR_FULL);
    }

    item_t front = {0};
    assert(list_status_ok(list_peek_front(&list, &front)));
    assert(front.id == 0u);

    assert(list_status_ok(list_pop_front(&list, NULL)));
    assert(list_size(&list) == 3u);

    list_clear(&list);
    assert(list_empty(&list));

    list_deinit(&list);
}

static void test_arena_dynamic_nodes(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    list_t list;
    assert(list_status_ok(list_init(&list, &(list_config_t){
        .elem_size = sizeof(item_t),
        .arena = &arena,
    })));

    for (uint32_t i = 0u; i < 10u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(list_status_ok(list_push_front(&list, &item)));
    }

    assert(list_size(&list) == 10u);

    item_t back = {0};
    assert(list_status_ok(list_peek_back(&list, &back)));
    assert(back.id == 0u);

    const item_t middle = { .id = 50u, .value = 500 };
    assert(list_status_ok(list_insert_at(&list, 5u, &middle)));

    item_t at = {0};
    assert(list_status_ok(list_peek_at(&list, 5u, &at)));
    assert(at.id == 50u);

    int32_t sum = 0;
    assert(list_status_ok(list_foreach(&list, sum_values, &sum)));

    const uint32_t remove_id = 50u;
    assert(list_status_ok(list_remove_first(&list, item_id_equals, &remove_id, NULL)));
    assert(list_size(&list) == 10u);

    list_deinit(&list);
    arena_deinit(&arena);
}

static void test_list_create(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    list_t *list = NULL;
    assert(list_status_ok(list_create(&list, sizeof(item_t), &arena, LIST_FLAG_NONE)));

    const item_t item = { .id = 1u, .value = 42 };
    assert(list_status_ok(list_push_back(list, &item)));
    assert(list_size(list) == 1u);

    list_destroy(list);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_dynamic_list(void)
{
    list_t *list = NULL;
    assert(list_status_ok(list_create(&list, sizeof(item_t), NULL, LIST_FLAG_NONE)));

    for (uint32_t i = 0u; i < 100u; ++i) {
        const item_t item = { .id = i, .value = (int32_t)i };
        assert(list_status_ok(list_push_back(list, &item)));
    }

    assert(list_size(list) == 100u);

    item_t popped = {0};
    assert(list_status_ok(list_pop_back(list, &popped)));
    assert(popped.id == 99u);

    list_destroy(list);
}
#endif

int main(void)
{
    test_caller_owned_node_pool();
    test_arena_dynamic_nodes();
    test_list_create();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_dynamic_list();
#endif

    puts("list: ok");
    return 0;
}
