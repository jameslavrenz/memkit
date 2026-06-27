#ifndef MEMKIT_BENCH_HAND_ROLLED_RING_BUFFER_H
#define MEMKIT_BENCH_HAND_ROLLED_RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct c_ring {
    int32_t*  storage;
    size_t    capacity;
    size_t    head;
    size_t    tail;
    size_t    count;
} c_ring_t;

int c_ring_init(c_ring_t* ring, int32_t* storage, size_t capacity);
void c_ring_clear(c_ring_t* ring);

int c_ring_push_back(c_ring_t* ring, int32_t value);
int c_ring_pop_front(c_ring_t* ring, int32_t* out_value);

size_t c_ring_size(const c_ring_t* ring);

#ifdef __cplusplus
}
#endif

#endif
