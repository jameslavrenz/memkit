#include <stdint.h>

#include "../hand_rolled/ring_buffer.h"

volatile int32_t sink = 0;

int main(void)
{
    int32_t storage[128];
    c_ring_t ring;
    c_ring_init(&ring, storage, 128u);

    for (int32_t i = 0; i < 4096; ++i) {
        (void)c_ring_push_back(&ring, i);
        int32_t out = 0;
        (void)c_ring_pop_front(&ring, &out);
        sink += out;
    }

    return 0;
}
