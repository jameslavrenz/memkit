/*
 * MPU-style usage: mmap-backed arena (default) and heap-backed ring.
 * Build with: make mpu  (or cmake -DMEMKIT_EMBEDDED_LINUX=ON)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ring.h"

typedef struct log_line {
    char message[64];
} log_line_t;

int main(void)
{
    /* arena_create() uses MEMKIT_DEFAULT_ARENA_BACKING (mmap on MPU). */
    arena_t *arena = NULL;
    assert(arena_status_ok(arena_create(&arena, 4096u)));

    arena_stats_t stats = {0};
    assert(arena_status_ok(arena_stats(arena, &stats)));
    printf("arena capacity=%zu bytes (mmap-backed by default on MPU)\n", stats.capacity_bytes);

    ring_t *logs = NULL;
    assert(ring_status_ok(ring_create(
        &logs,
        sizeof(log_line_t),
        32u,
        NULL,
        RING_FLAG_OVERWRITE_ON_FULL
    )));

    for (int i = 0; i < 40; ++i) {
        log_line_t line = {0};
        snprintf(line.message, sizeof line.message, "event %d", i);
        assert(ring_status_ok(ring_push_back(logs, &line)));
    }

    printf("log ring size=%zu (capacity=%zu)\n", ring_size(logs), ring_capacity(logs));

    log_line_t front = {0};
    assert(ring_status_ok(ring_peek_front(logs, &front)));
    printf("oldest retained log: %s\n", front.message);

    ring_destroy(logs);
    arena_destroy(arena);

    return 0;
}
