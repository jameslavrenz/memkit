#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "list.h"

int main(void)
{
    list_t list;
    assert(list_status_ok(list_init(&list, &(list_config_t){
        .elem_size = sizeof(uint32_t),
        .flags     = LIST_FLAG_DYNAMIC_STORAGE,
    })));

    for (uint32_t i = 0u; i < 4u; ++i) {
        assert(list_status_ok(list_push_back(&list, &i)));
    }

    assert(list_size(&list) == 4u);

    uint32_t peek = 0u;
    assert(list_status_ok(list_peek_front(&list, &peek)));
    assert(peek == 0u);
    assert(list_status_ok(list_peek_back(&list, &peek)));
    assert(peek == 3u);

    assert(list_status_ok(list_pop_front(&list, &peek)));
    assert(peek == 0u);

    const uint32_t inserted = 99u;
    assert(list_status_ok(list_insert_at(&list, 1u, &inserted)));
    assert(list_status_ok(list_peek_at(&list, 1u, &peek)));
    assert(peek == 99u);

    assert(list_status_ok(list_remove_at(&list, 1u, &peek)));
    assert(peek == 99u);

    list_clear(&list);
    assert(list_empty(&list));
    list_deinit(&list);

    printf("list_c: ok\n");
    return 0;
}
