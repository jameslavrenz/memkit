#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "dlist.h"

int main(void)
{
    dlist_t dlist;
    assert(dlist_status_ok(dlist_init(&dlist, &(dlist_config_t){
        .elem_size = sizeof(uint32_t),
        .flags     = DLIST_FLAG_DYNAMIC_STORAGE,
    })));

    for (uint32_t i = 0u; i < 4u; ++i) {
        assert(dlist_status_ok(dlist_push_back(&dlist, &i)));
    }

    assert(dlist_size(&dlist) == 4u);

    uint32_t peek = 0u;
    assert(dlist_status_ok(dlist_peek_front(&dlist, &peek)));
    assert(peek == 0u);
    assert(dlist_status_ok(dlist_peek_back(&dlist, &peek)));
    assert(peek == 3u);

    assert(dlist_status_ok(dlist_pop_front(&dlist, &peek)));
    assert(peek == 0u);
    assert(dlist_status_ok(dlist_pop_back(&dlist, &peek)));
    assert(peek == 3u);

    const uint32_t front_val = 100u;
    const uint32_t back_val = 200u;
    assert(dlist_status_ok(dlist_push_front(&dlist, &front_val)));
    assert(dlist_status_ok(dlist_push_back(&dlist, &back_val)));

    assert(*(const uint32_t *)dlist_front_const(&dlist) == 100u);
    assert(*(const uint32_t *)dlist_back_const(&dlist) == 200u);

    dlist_clear(&dlist);
    assert(dlist_empty(&dlist));
    dlist_deinit(&dlist);

    printf("dlist_c: ok\n");
    return 0;
}
