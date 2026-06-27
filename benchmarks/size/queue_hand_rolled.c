#include <stdint.h>

#include "../hand_rolled/fifo_queue.h"

volatile int32_t sink = 0;

int main(void)
{
    int32_t storage[128];
    c_fifo_t queue;
    c_fifo_init(&queue, storage, 128u);

    for (int32_t i = 0; i < 4096; ++i) {
        (void)c_fifo_push(&queue, i);
        int32_t out = 0;
        (void)c_fifo_pop(&queue, &out);
        sink += out;
    }

    return 0;
}
