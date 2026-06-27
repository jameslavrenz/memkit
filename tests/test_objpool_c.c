#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "objpool.h"

typedef struct block {
    uint32_t id;
    int32_t value;
} block_t;

static void test_caller_owned(void)
{
    enum { capacity = 4u };

    uint8_t storage[objpool_storage_bytes(sizeof(block_t), capacity)];
    uint32_t free_stack[capacity];
    uint8_t used_bits[objpool_used_bits_bytes(capacity)];

    objpool_t pool;
    assert(objpool_status_ok(objpool_init(&pool, &(objpool_config_t){
        .elem_size        = sizeof(block_t),
        .capacity         = capacity,
        .storage          = storage,
        .storage_bytes    = sizeof storage,
        .free_stack       = free_stack,
        .free_stack_bytes = sizeof free_stack,
        .used_bits        = used_bits,
        .used_bits_bytes  = sizeof used_bits,
    })));

    assert(objpool_empty(&pool));
    assert(objpool_available(&pool) == capacity);

    block_t *slots[capacity];
    memset(slots, 0, sizeof slots);

    for (uint32_t i = 0u; i < capacity; ++i) {
        assert(objpool_status_ok(objpool_alloc(&pool, (void **)&slots[i])));
        slots[i]->id = i;
        slots[i]->value = (int32_t)(i * 10);
    }

    assert(objpool_full(&pool));

    void *extra = NULL;
    assert(objpool_alloc(&pool, &extra) == OBJPOOL_ERR_FULL);

    for (uint32_t i = 0u; i < capacity; ++i) {
        assert(objpool_contains(&pool, slots[i]));
    }

    assert(objpool_status_ok(objpool_free(&pool, slots[2])));
    assert(objpool_available(&pool) == 1u);

    const block_t source = {.id = 42u, .value = 420};
    block_t *reused = NULL;
    assert(objpool_status_ok(objpool_alloc_copy(&pool, &source, (void **)&reused)));
    assert(reused == slots[2]);
    assert(reused->id == 42u);

    objpool_clear(&pool);
    assert(objpool_empty(&pool));

    objpool_deinit(&pool);
}

static void test_arena_create(void)
{
    static uint8_t arena_backing[4096u];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing       = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    objpool_t *pool = NULL;
    assert(objpool_status_ok(objpool_create(
        &pool,
        sizeof(block_t),
        8u,
        &arena,
        0u
    )));

    block_t *slot = NULL;
    assert(objpool_status_ok(objpool_alloc(pool, (void **)&slot)));
    slot->id = 1u;

    objpool_destroy(pool);
    arena_deinit(&arena);
}

int main(void)
{
    test_caller_owned();
    test_arena_create();
    printf("objpool_c: ok\n");
    return 0;
}
