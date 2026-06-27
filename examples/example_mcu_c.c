/*
 * MCU-style usage: C API tier-1 containers with static backing (no heap).
 */

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "queue.h"
#include "ring.h"

typedef struct sensor_sample {
    uint32_t timestamp_ms;
    int16_t  value;
} sensor_sample_t;

int main(void)
{
    static uint8_t ring_storage[sizeof(sensor_sample_t) * 16u];
    static uint8_t queue_storage[sizeof(sensor_sample_t) * 8u];

    ring_t ring;
    assert(ring_status_ok(ring_init(&ring, &(ring_config_t){
        .elem_size     = sizeof(sensor_sample_t),
        .capacity      = 16u,
        .storage       = ring_storage,
        .storage_bytes = sizeof ring_storage,
    })));

    queue_t queue;
    assert(queue_status_ok(queue_init(&queue, &(queue_config_t){
        .elem_size     = sizeof(sensor_sample_t),
        .capacity      = 8u,
        .storage       = queue_storage,
        .storage_bytes = sizeof queue_storage,
    })));

    for (uint32_t i = 0u; i < 16u; ++i) {
        const sensor_sample_t sample = {
            .timestamp_ms = i * 10u,
            .value        = (int16_t)(i * 3),
        };
        assert(ring_status_ok(ring_push_back(&ring, &sample)));
        if (i >= 8u) {
            continue;
        }
        assert(queue_status_ok(queue_push(&queue, &sample)));
    }

    printf("ring size=%zu capacity=%zu\n", ring_size(&ring), ring_capacity(&ring));
    printf("queue size=%zu capacity=%zu\n", queue_size(&queue), queue_capacity(&queue));

    sensor_sample_t oldest = {0};
    assert(ring_status_ok(ring_peek_front(&ring, &oldest)));
    printf(
        "oldest ring sample: ts=%" PRIu32 " value=%d\n",
        oldest.timestamp_ms,
        (int)oldest.value
    );

    while (!queue_empty(&queue)) {
        sensor_sample_t out = {0};
        assert(queue_status_ok(queue_pop(&queue, &out)));
    }

    ring_deinit(&ring);
    queue_deinit(&queue);

    printf("mcu_c: ok\n");
    return 0;
}
