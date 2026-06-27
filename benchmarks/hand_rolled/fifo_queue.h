#ifndef MEMKIT_BENCH_HAND_ROLLED_FIFO_QUEUE_H
#define MEMKIT_BENCH_HAND_ROLLED_FIFO_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct c_fifo {
    int32_t* storage;
    size_t   capacity;
    size_t   head;
    size_t   tail;
    size_t   count;
} c_fifo_t;

int c_fifo_init(c_fifo_t* queue, int32_t* storage, size_t capacity);
void c_fifo_clear(c_fifo_t* queue);

int c_fifo_push(c_fifo_t* queue, int32_t value);
int c_fifo_pop(c_fifo_t* queue, int32_t* out_value);

size_t c_fifo_size(const c_fifo_t* queue);

#ifdef __cplusplus
}
#endif

#endif
