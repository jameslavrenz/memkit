#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "handle_pool.h"

static void test_caller_owned(void)
{
    enum { capacity = 4u };

    uint8_t storage[handle_pool_storage_bytes(sizeof(uint32_t), capacity)];
    uint16_t generations[capacity];
    uint32_t free_stack[capacity];

    handle_pool_t pool;
    assert(handle_pool_status_ok(handle_pool_init(&pool, &(handle_pool_config_t){
        .elem_size          = sizeof(uint32_t),
        .capacity           = capacity,
        .storage            = storage,
        .storage_bytes      = sizeof storage,
        .generations        = generations,
        .generations_bytes  = sizeof generations,
        .free_stack         = free_stack,
        .free_stack_bytes   = sizeof free_stack,
    })));

    handle_t h0 = HANDLE_POOL_INVALID_HANDLE;
    handle_t h1 = HANDLE_POOL_INVALID_HANDLE;

    void *slot0 = NULL;
    void *slot1 = NULL;

    assert(handle_pool_status_ok(handle_pool_acquire(&pool, &slot0, &h0)));
    assert(handle_pool_status_ok(handle_pool_acquire(&pool, &slot1, &h1)));
    assert(h0 != h1);
    assert(handle_pool_valid(&pool, h0));
    assert(handle_pool_valid(&pool, h1));

    *(uint32_t *)slot0 = 100u;
    *(uint32_t *)slot1 = 200u;

    void *got = NULL;
    assert(handle_pool_status_ok(handle_pool_get(&pool, h0, &got)));
    assert(*(uint32_t *)got == 100u);

    assert(handle_pool_status_ok(handle_pool_release(&pool, h0)));
    assert(!handle_pool_valid(&pool, h0));
    assert(handle_pool_valid(&pool, h1));

    handle_t h2 = HANDLE_POOL_INVALID_HANDLE;
    void *slot2 = NULL;
    assert(handle_pool_status_ok(handle_pool_acquire(&pool, &slot2, &h2)));
    assert(h2 != h0);
    *(uint32_t *)slot2 = 300u;

    handle_pool_deinit(&pool);
}

static void test_arena_create(void)
{
    static uint8_t arena_backing[2048u];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing       = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    handle_pool_t *pool = NULL;
    assert(handle_pool_status_ok(handle_pool_create(
        &pool,
        sizeof(int32_t),
        8u,
        &arena,
        0u
    )));

    handle_t handle = HANDLE_POOL_INVALID_HANDLE;
    void *slot = NULL;
    assert(handle_pool_status_ok(handle_pool_acquire(pool, &slot, &handle)));
    *(int32_t *)slot = -42;

    void *got = NULL;
    assert(handle_pool_status_ok(handle_pool_get(pool, handle, &got)));
    assert(*(int32_t *)got == -42);

    handle_pool_destroy(pool);
    arena_deinit(&arena);
}

int main(void)
{
    test_caller_owned();
    test_arena_create();
    printf("handle_pool_c: ok\n");
    return 0;
}
