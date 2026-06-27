#include "fifo_queue.h"

int c_fifo_init(c_fifo_t* queue, int32_t* storage, size_t capacity)
{
    if (queue == NULL || storage == NULL || capacity == 0u) {
        return -1;
    }

    queue->storage  = storage;
    queue->capacity = capacity;
    queue->head     = 0u;
    queue->tail     = 0u;
    queue->count    = 0u;
    return 0;
}

void c_fifo_clear(c_fifo_t* queue)
{
    if (queue == NULL) {
        return;
    }

    queue->head  = 0u;
    queue->tail  = 0u;
    queue->count = 0u;
}

int c_fifo_push(c_fifo_t* queue, int32_t value)
{
    if (queue == NULL || queue->count >= queue->capacity) {
        return -1;
    }

    queue->storage[queue->tail] = value;
    queue->tail                 = (queue->tail + 1u) % queue->capacity;
    ++queue->count;
    return 0;
}

int c_fifo_pop(c_fifo_t* queue, int32_t* out_value)
{
    if (queue == NULL || queue->count == 0u) {
        return -1;
    }

    if (out_value != NULL) {
        *out_value = queue->storage[queue->head];
    }

    queue->head = (queue->head + 1u) % queue->capacity;
    --queue->count;
    return 0;
}

size_t c_fifo_size(const c_fifo_t* queue)
{
    return queue != NULL ? queue->count : 0u;
}
