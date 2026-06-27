#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "pqueue.h"

static int compare_u32(const void *a, const void *b, void *user)
{
    (void)user;
    const uint32_t av = *(const uint32_t *)a;
    const uint32_t bv = *(const uint32_t *)b;
    if (av < bv) {
        return -1;
    }
    if (av > bv) {
        return 1;
    }
    return 0;
}

int main(void)
{
    pqueue_t pqueue;
    assert(pqueue_status_ok(pqueue_init(&pqueue, &(pqueue_config_t){
        .elem_size  = sizeof(uint32_t),
        .capacity   = 8u,
        .compare_fn = compare_u32,
        .flags      = PQUEUE_FLAG_DYNAMIC_STORAGE | PQUEUE_FLAG_GROWABLE,
    })));

    const uint32_t values[] = {5u, 1u, 9u, 3u, 2u};
    for (size_t i = 0u; i < 5u; ++i) {
        assert(pqueue_status_ok(pqueue_push(&pqueue, &values[i])));
    }

    uint32_t top = 0u;
    assert(pqueue_status_ok(pqueue_peek(&pqueue, &top)));
    assert(top == 1u);

    const uint32_t expected[] = {1u, 2u, 3u, 5u, 9u};
    for (size_t i = 0u; i < 5u; ++i) {
        uint32_t out = 0u;
        assert(pqueue_status_ok(pqueue_pop(&pqueue, &out)));
        assert(out == expected[i]);
    }

    assert(pqueue_empty(&pqueue));
    pqueue_deinit(&pqueue);

    printf("pqueue_c: ok\n");
    return 0;
}
