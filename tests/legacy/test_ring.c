#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "ring.h"

typedef struct packet {
    uint32_t id;
    uint8_t payload[8];
} packet_t;

static void test_caller_owned_static(void)
{
    static uint8_t storage[sizeof(packet_t) * 4u];

    ring_t ring;
    const ring_config_t config = {
        .elem_size = sizeof(packet_t),
        .capacity = 4u,
        .storage = storage,
        .storage_bytes = sizeof storage,
    };

    assert(ring_status_ok(ring_init(&ring, &config)));
    assert(ring_empty(&ring));
    assert(ring_capacity(&ring) == 4u);

    for (uint32_t i = 0u; i < 4u; ++i) {
        const packet_t packet = { .id = i };
        assert(ring_status_ok(ring_push_back(&ring, &packet)));
    }

    assert(ring_full(&ring));
    assert(ring_size(&ring) == 4u);

    packet_t popped = {0};
    assert(ring_status_ok(ring_pop_front(&ring, &popped)));
    assert(popped.id == 0u);
    assert(ring_size(&ring) == 3u);

    ring_clear(&ring);
    assert(ring_empty(&ring));

    ring_deinit(&ring);
}

static void test_arena_owned_ring(void)
{
    static uint8_t arena_backing[1024];

    arena_t arena;
    const arena_config_t arena_config = {
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    };
    assert(arena_status_ok(arena_init(&arena, &arena_config)));

    ring_t *ring = NULL;
    assert(ring_status_ok(ring_create(
        &ring,
        sizeof(packet_t),
        8u,
        &arena,
        RING_FLAG_OVERWRITE_ON_FULL
    )));

    for (uint32_t i = 0u; i < 4u; ++i) {
        const packet_t packet = { .id = i };
        assert(ring_status_ok(ring_push_back(ring, &packet)));
    }

    void *write_ptr = NULL;
    const size_t writable = ring_writable_contiguous(ring, &write_ptr);
    assert(writable > 0u);
    assert(write_ptr != NULL);

    for (uint32_t i = 4u; i < 12u; ++i) {
        const packet_t packet = { .id = i };
        assert(ring_status_ok(ring_push_back(ring, &packet)));
    }

    assert(ring_size(ring) == 8u);

    packet_t newest = {0};
    assert(ring_status_ok(ring_peek_back(ring, &newest)));
    assert(newest.id == 11u);

    ring_destroy(ring);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_dynamic_ring(void)
{
    ring_t *ring = NULL;
    assert(ring_status_ok(ring_create(
        &ring,
        sizeof(packet_t),
        4u,
        NULL,
        RING_FLAG_NONE
    )));

    const packet_t packet = { .id = 42u };
    assert(ring_status_ok(ring_push_back(ring, &packet)));
    assert(ring_size(ring) == 1u);

    ring_destroy(ring);
}
#endif

int main(void)
{
    test_caller_owned_static();
    test_arena_owned_ring();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_dynamic_ring();
#endif
    return 0;
}
