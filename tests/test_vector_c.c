#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "vector.h"

static void test_caller_owned(void)
{
    static uint8_t storage[sizeof(int32_t) * 4u];

    vector_t vector;
    assert(vector_status_ok(vector_init(&vector, &(vector_config_t){
        .elem_size     = sizeof(int32_t),
        .capacity      = 4u,
        .storage       = storage,
        .storage_bytes = sizeof storage,
    })));

    for (int32_t i = 0; i < 4; ++i) {
        assert(vector_status_ok(vector_push_back(&vector, &i)));
    }

    assert(vector_size(&vector) == 4u);

    int32_t back = 0;
    assert(vector_status_ok(vector_peek_back(&vector, &back)));
    assert(back == 3);

    assert(vector_status_ok(vector_pop_back(&vector, &back)));
    assert(back == 3);
    assert(vector_size(&vector) == 3u);

    vector_deinit(&vector);
}

static void test_arena_growable(void)
{
    static uint8_t arena_backing[4096u];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing       = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    vector_t vector;
    assert(vector_status_ok(vector_init(&vector, &(vector_config_t){
        .elem_size     = sizeof(int32_t),
        .capacity      = 2u,
        .arena         = &arena,
        .flags         = VECTOR_FLAG_ARENA_STORAGE | VECTOR_FLAG_GROWABLE,
    })));

    for (int32_t i = 0; i < 10; ++i) {
        assert(vector_status_ok(vector_push_back(&vector, &i)));
    }

    assert(vector_size(&vector) == 10u);

    int32_t at = 0;
    assert(vector_status_ok(vector_peek_at(&vector, 5u, &at)));
    assert(at == 5);

    vector_deinit(&vector);
    arena_deinit(&arena);
}

int main(void)
{
    test_caller_owned();
    test_arena_growable();
    printf("vector_c: ok\n");
    return 0;
}
