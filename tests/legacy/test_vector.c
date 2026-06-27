#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "vector.h"

typedef struct packet {
    uint32_t id;
    uint8_t payload[8];
} packet_t;

static void test_caller_owned_fixed(void)
{
    static uint8_t storage[sizeof(packet_t) * 4u];

    vector_t vector;
    const vector_config_t config = {
        .elem_size = sizeof(packet_t),
        .capacity = 4u,
        .storage = storage,
        .storage_bytes = sizeof storage,
    };

    assert(vector_status_ok(vector_init(&vector, &config)));
    assert(vector_empty(&vector));
    assert(vector_capacity(&vector) == 4u);

    for (uint32_t i = 0u; i < 4u; ++i) {
        const packet_t packet = { .id = i };
        assert(vector_status_ok(vector_push_back(&vector, &packet)));
    }

    assert(vector_size(&vector) == 4u);
    {
        const packet_t extra = { .id = 99u };
        assert(vector_push_back(&vector, &extra) == VECTOR_ERR_FULL);
    }

    packet_t back = {0};
    assert(vector_status_ok(vector_peek_back(&vector, &back)));
    assert(back.id == 3u);

    vector_clear(&vector);
    assert(vector_empty(&vector));

    vector_deinit(&vector);
}

static void test_caller_owned_growable_with_arena(void)
{
    static uint8_t arena_backing[4096];
    static uint8_t storage[sizeof(packet_t) * 2u];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    vector_t vector;
    assert(vector_status_ok(vector_init(&vector, &(vector_config_t){
        .elem_size = sizeof(packet_t),
        .capacity = 2u,
        .storage = storage,
        .storage_bytes = sizeof storage,
        .arena = &arena,
        .flags = VECTOR_FLAG_GROWABLE,
    })));

    for (uint32_t i = 0u; i < 10u; ++i) {
        const packet_t packet = { .id = i };
        assert(vector_status_ok(vector_push_back(&vector, &packet)));
    }

    assert(vector_size(&vector) == 10u);
    assert(vector_capacity(&vector) >= 10u);

    packet_t front = {0};
    assert(vector_status_ok(vector_peek_front(&vector, &front)));
    assert(front.id == 0u);

    vector_deinit(&vector);
    arena_deinit(&arena);
}

static void test_arena_owned_vector(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    vector_t *vector = NULL;
    assert(vector_status_ok(vector_create(
        &vector,
        sizeof(packet_t),
        2u,
        &arena,
        VECTOR_FLAG_NONE
    )));

    size_t last_capacity = vector_capacity(vector);
    for (uint32_t i = 0u; i < 20u; ++i) {
        const packet_t packet = { .id = i };
        assert(vector_status_ok(vector_push_back(vector, &packet)));
        if (vector_capacity(vector) > last_capacity) {
            assert(vector_capacity(vector) == last_capacity * 2u);
            last_capacity = vector_capacity(vector);
        }
    }

    assert(vector_size(vector) == 20u);

    vector_destroy(vector);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_dynamic_vector(void)
{
    vector_t *vector = NULL;
    assert(vector_status_ok(vector_create(
        &vector,
        sizeof(packet_t),
        1u,
        NULL,
        VECTOR_FLAG_NONE
    )));

    for (uint32_t i = 0u; i < 100u; ++i) {
        const packet_t packet = { .id = i };
        assert(vector_status_ok(vector_push_back(vector, &packet)));
    }

    assert(vector_size(vector) == 100u);
    assert(vector_capacity(vector) >= 100u);

    for (uint32_t i = 0u; i < 50u; ++i) {
        assert(vector_status_ok(vector_pop_back(vector, NULL)));
    }

    assert(vector_size(vector) == 50u);

    vector_destroy(vector);
}
#endif

int main(void)
{
    test_caller_owned_fixed();
    test_caller_owned_growable_with_arena();
    test_arena_owned_vector();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_dynamic_vector();
#endif
    return 0;
}
