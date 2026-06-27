#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "objpool.h"

typedef struct {
    uint32_t id;
    int32_t value;
} block_t;

static void block_destroy(void *elem, void *user)
{
    (void)user;
    block_t *block = (block_t *)elem;
    block->id = 0u;
    block->value = -1;
}

static objpool_status_t sum_values(const void *elem, size_t index, void *user)
{
    (void)index;
    const block_t *block = (const block_t *)elem;
    int32_t *sum = (int32_t *)user;
    *sum += block->value;
    return OBJPOOL_OK;
}

static void test_objpool_caller_owned(void)
{
    enum { CAPACITY = 4u };

    static uint8_t storage[sizeof(block_t) * CAPACITY];
    static uint32_t free_stack[CAPACITY];
    static uint8_t used_bits[(CAPACITY + 7u) / 8u];

    objpool_t pool;
    assert(objpool_status_ok(objpool_init(&pool, &(objpool_config_t){
        .elem_size = sizeof(block_t),
        .capacity = CAPACITY,
        .storage = storage,
        .storage_bytes = sizeof storage,
        .free_stack = free_stack,
        .free_stack_bytes = sizeof free_stack,
        .used_bits = used_bits,
        .used_bits_bytes = sizeof used_bits,
        .destroy_fn = block_destroy,
    })));

    assert(objpool_empty(&pool));
    assert(objpool_capacity(&pool) == CAPACITY);
    assert(objpool_available(&pool) == CAPACITY);

    block_t *slots[CAPACITY] = {0};
    for (uint32_t i = 0u; i < CAPACITY; ++i) {
        assert(objpool_status_ok(objpool_alloc(&pool, (void **)&slots[i])));
        slots[i]->id = i;
        slots[i]->value = (int32_t)(i * 10);
    }

    assert(objpool_full(&pool));
    assert(objpool_size(&pool) == CAPACITY);

    block_t *extra = NULL;
    assert(objpool_alloc(&pool, (void **)&extra) == OBJPOOL_ERR_FULL);

    for (uint32_t i = 0u; i < CAPACITY; ++i) {
        assert(objpool_contains(&pool, slots[i]));
    }

    assert(objpool_status_ok(objpool_free(&pool, slots[2])));
    assert(slots[2]->value == -1);
    assert(objpool_available(&pool) == 1u);

    block_t source = { .id = 42u, .value = 420 };
    block_t *reused = NULL;
    assert(objpool_status_ok(objpool_alloc_copy(&pool, &source, (void **)&reused)));
    assert(reused == slots[2]);
    assert(reused->id == 42u);
    assert(reused->value == 420);

    int32_t sum = 0;
    assert(objpool_status_ok(objpool_foreach(&pool, sum_values, &sum)));
    assert(sum == 460);

    objpool_clear(&pool);
    assert(objpool_empty(&pool));
    assert(objpool_available(&pool) == CAPACITY);

    objpool_deinit(&pool);
}

static void test_objpool_arena(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    objpool_t *pool = NULL;
    assert(objpool_status_ok(objpool_create(
        &pool,
        sizeof(block_t),
        8u,
        &arena,
        OBJPOOL_FLAG_NONE
    )));

    block_t *first = NULL;
    block_t *second = NULL;
    assert(objpool_status_ok(objpool_alloc(pool, (void **)&first)));
    assert(objpool_status_ok(objpool_alloc(pool, (void **)&second)));

    first->id = 1u;
    first->value = 100;
    second->id = 2u;
    second->value = 200;

    size_t index = 0u;
    assert(objpool_status_ok(objpool_index(pool, second, &index)));
    assert(index == 1u);

    assert(objpool_status_ok(objpool_free(pool, first)));
    assert(!objpool_contains(pool, first));

    block_t *third = NULL;
    assert(objpool_status_ok(objpool_alloc(pool, (void **)&third)));
    assert(third == first);

    objpool_destroy(pool);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_objpool_dynamic(void)
{
    objpool_t *pool = NULL;
    assert(objpool_status_ok(objpool_create(
        &pool,
        sizeof(block_t),
        16u,
        NULL,
        OBJPOOL_FLAG_NONE
    )));

    block_t *slots[16] = {0};
    for (size_t i = 0u; i < 16u; ++i) {
        assert(objpool_status_ok(objpool_alloc(pool, (void **)&slots[i])));
        slots[i]->id = (uint32_t)i;
        slots[i]->value = (int32_t)i;
    }

    for (size_t i = 0u; i < 16u; i += 2u) {
        assert(objpool_status_ok(objpool_free(pool, slots[i])));
    }

    assert(objpool_size(pool) == 8u);
    assert(objpool_available(pool) == 8u);

    for (size_t i = 0u; i < 8u; ++i) {
        block_t *slot = NULL;
        assert(objpool_status_ok(objpool_alloc(pool, (void **)&slot)));
        slot->id = (uint32_t)(100u + i);
    }

    assert(objpool_full(pool));
    objpool_destroy(pool);
}
#endif

int main(void)
{
    test_objpool_caller_owned();
    test_objpool_arena();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_objpool_dynamic();
#endif

    puts("objpool: ok");
    return 0;
}
