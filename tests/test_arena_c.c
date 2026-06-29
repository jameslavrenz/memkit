#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <limits.h>

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

    void *too_large = NULL;
    assert(arena_alloc(&arena, sizeof backing, 8u, &too_large) == ARENA_ERR_OOM);
    assert(too_large == NULL);

    arena_reset(&arena);
    assert(arena_status_ok(arena_stats(&arena, &stats)));
    assert(stats.used_bytes == 0u);
    assert(stats.allocation_count == 0u);

    void *after_reset = NULL;
    assert(arena_status_ok(arena_calloc(&arena, 4u, 4u, 8u, &after_reset)));
    assert(after_reset != NULL);

    void *calloc_overflow = NULL;
    assert(arena_calloc(&arena, 2u, SIZE_MAX, 8u, &calloc_overflow) == ARENA_ERR_INVALID);
    assert(calloc_overflow == NULL);

    static uint8_t tight_backing[16u];
    arena_t tight;
    assert(arena_status_ok(arena_init(&tight, &(arena_config_t){
        .backing       = tight_backing,
        .backing_bytes = sizeof tight_backing,
    })));

    void *first = NULL;
    assert(arena_status_ok(arena_alloc(&tight, 9u, 8u, &first)));

    void *second = NULL;
    assert(arena_alloc(&tight, 8u, 8u, &second) == ARENA_ERR_OOM);
    assert(second == NULL);

    arena_deinit(&arena);
    arena_deinit(&tight);

    printf("arena_c: ok\n");
    return 0;
}
