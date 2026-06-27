#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"

int main(void)
{
    static uint8_t backing[512u];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing       = backing,
        .backing_bytes = sizeof backing,
    })));

    void *ptr_a = NULL;
    void *ptr_b = NULL;
    assert(arena_status_ok(arena_alloc(&arena, 16u, 8u, &ptr_a)));
    assert(arena_status_ok(arena_alloc(&arena, 32u, 8u, &ptr_b)));
    assert(ptr_a != NULL);
    assert(ptr_b != NULL);
    assert(ptr_a != ptr_b);

    arena_stats_t stats = {0};
    assert(arena_status_ok(arena_stats(&arena, &stats)));
    assert(stats.used_bytes > 0u);
    assert(stats.remaining_bytes < stats.capacity_bytes);
    assert(stats.allocation_count == 2u);

    arena_reset(&arena);
    assert(arena_status_ok(arena_stats(&arena, &stats)));
    assert(stats.used_bytes == 0u);
    assert(stats.allocation_count == 0u);

    void *after_reset = NULL;
    assert(arena_status_ok(arena_calloc(&arena, 4u, 4u, 8u, &after_reset)));
    assert(after_reset != NULL);

    arena_deinit(&arena);

    printf("arena_c: ok\n");
    return 0;
}
