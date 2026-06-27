#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "bitset.h"

static void test_caller_owned(void)
{
    enum { pin_count = 10u };
    uint8_t storage[bitset_storage_bytes(pin_count)];

    bitset_t pins;
    assert(bitset_status_ok(bitset_init(&pins, &(bitset_config_t){
        .capacity      = pin_count,
        .storage       = storage,
        .storage_bytes = sizeof storage,
    })));
    assert(bitset_empty(&pins));

    assert(bitset_status_ok(bitset_set(&pins, 1u)));
    assert(bitset_status_ok(bitset_set(&pins, 3u)));
    assert(bitset_status_ok(bitset_set(&pins, 7u)));
    assert(bitset_size(&pins) == 3u);
    assert(bitset_test(&pins, 3u));
    assert(!bitset_test(&pins, 4u));

    assert(bitset_status_ok(bitset_toggle(&pins, 3u)));
    assert(!bitset_test(&pins, 3u));

    assert(bitset_status_ok(bitset_set_all(&pins)));
    assert(bitset_full(&pins));

    bitset_clear(&pins);
    assert(bitset_empty(&pins));

    size_t pin = 0u;
    assert(bitset_status_ok(bitset_find_first_clear(&pins, 0u, &pin)));
    assert(pin == 0u);

    bitset_deinit(&pins);
}

static void test_logical_ops(void)
{
    enum { capacity = 8u };
    static uint8_t left_storage[1];
    static uint8_t right_storage[1];
    static uint8_t tmp_storage[1];

    bitset_t left;
    bitset_t right;
    bitset_t tmp;

    assert(bitset_status_ok(bitset_init(&left, &(bitset_config_t){
        .capacity = capacity, .storage = left_storage, .storage_bytes = sizeof left_storage,
    })));
    assert(bitset_status_ok(bitset_init(&right, &(bitset_config_t){
        .capacity = capacity, .storage = right_storage, .storage_bytes = sizeof right_storage,
    })));
    assert(bitset_status_ok(bitset_init(&tmp, &(bitset_config_t){
        .capacity = capacity, .storage = tmp_storage, .storage_bytes = sizeof tmp_storage,
    })));

    assert(bitset_status_ok(bitset_set(&left, 1u)));
    assert(bitset_status_ok(bitset_set(&left, 2u)));
    assert(bitset_status_ok(bitset_set(&left, 5u)));
    assert(bitset_status_ok(bitset_set(&right, 2u)));
    assert(bitset_status_ok(bitset_set(&right, 3u)));
    assert(bitset_status_ok(bitset_set(&right, 5u)));

    assert(bitset_status_ok(bitset_copy(&tmp, &left)));
    assert(bitset_status_ok(bitset_intersect_with(&tmp, &right)));
    assert(bitset_test(&tmp, 2u));
    assert(bitset_test(&tmp, 5u));
    assert(!bitset_test(&tmp, 1u));
    assert(bitset_size(&tmp) == 2u);

    bitset_deinit(&left);
    bitset_deinit(&right);
    bitset_deinit(&tmp);
}

int main(void)
{
    test_caller_owned();
    test_logical_ops();
    printf("bitset_c: ok\n");
    return 0;
}
