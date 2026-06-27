#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "ring.h"

typedef struct packet {
    uint32_t id;
    uint8_t payload[8];
} packet_t;

static void test_caller_owned(void)
{
    static uint8_t storage[sizeof(packet_t) * 4u];

    ring_t ring;
    assert(ring_status_ok(ring_init(&ring, &(ring_config_t){
        .elem_size     = sizeof(packet_t),
        .capacity      = 4u,
        .storage       = storage,
        .storage_bytes = sizeof storage,
    })));
    assert(ring_empty(&ring));
    assert(ring_capacity(&ring) == 4u);

    for (uint32_t i = 0u; i < 4u; ++i) {
        const packet_t packet = {.id = i};
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

static void test_arena_owned(void)
{
    static uint8_t arena_backing[1024u];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing       = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    ring_t *ring = NULL;
    assert(ring_status_ok(ring_create(
        &ring,
        sizeof(packet_t),
        8u,
        &arena,
        RING_FLAG_OVERWRITE_ON_FULL
    )));

    for (uint32_t i = 0u; i < 12u; ++i) {
        const packet_t packet = {.id = i};
        assert(ring_status_ok(ring_push_back(ring, &packet)));
    }

    assert(ring_size(ring) == 8u);

    const void *rx = NULL;
    const size_t rx_elems = ring_readable_contiguous(ring, &rx);
    assert(rx_elems > 0u);
    assert(rx != NULL);

    packet_t newest = {0};
    assert(ring_status_ok(ring_peek_back(ring, &newest)));
    assert(newest.id == 11u);

    ring_destroy(ring);
    arena_deinit(&arena);
}

int main(void)
{
    test_caller_owned();
    test_arena_owned();
    printf("ring_c: ok\n");
    return 0;
}
