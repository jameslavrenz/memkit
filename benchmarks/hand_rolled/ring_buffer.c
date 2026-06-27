#include "ring_buffer.h"

int c_ring_init(c_ring_t* ring, int32_t* storage, size_t capacity)
{
    if (ring == NULL || storage == NULL || capacity == 0u) {
        return -1;
    }

    ring->storage  = storage;
    ring->capacity = capacity;
    ring->head     = 0u;
    ring->tail     = 0u;
    ring->count    = 0u;
    return 0;
}

void c_ring_clear(c_ring_t* ring)
{
    if (ring == NULL) {
        return;
    }

    ring->head  = 0u;
    ring->tail  = 0u;
    ring->count = 0u;
}

int c_ring_push_back(c_ring_t* ring, int32_t value)
{
    if (ring == NULL || ring->count >= ring->capacity) {
        return -1;
    }

    ring->storage[ring->tail] = value;
    ring->tail                = (ring->tail + 1u) % ring->capacity;
    ++ring->count;
    return 0;
}

int c_ring_pop_front(c_ring_t* ring, int32_t* out_value)
{
    if (ring == NULL || ring->count == 0u) {
        return -1;
    }

    if (out_value != NULL) {
        *out_value = ring->storage[ring->head];
    }

    ring->head = (ring->head + 1u) % ring->capacity;
    --ring->count;
    return 0;
}

size_t c_ring_size(const c_ring_t* ring)
{
    return ring != NULL ? ring->count : 0u;
}
