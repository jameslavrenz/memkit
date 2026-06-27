#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "bitset.h"

static void test_bitset_caller_owned(void)
{
    enum { PIN_COUNT = 10u };

    static uint8_t storage[(PIN_COUNT + 7u) / 8u];

    bitset_t pins;
    assert(bitset_status_ok(bitset_init(&pins, &(bitset_config_t){
        .capacity = PIN_COUNT,
        .storage = storage,
        .storage_bytes = sizeof storage,
    })));

    assert(bitset_empty(&pins));
    assert(bitset_capacity(&pins) == PIN_COUNT);

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
    assert(bitset_size(&pins) == PIN_COUNT);

    bitset_clear(&pins);
    assert(bitset_empty(&pins));

    size_t pin = 0u;
    assert(bitset_status_ok(bitset_find_first_clear(&pins, 0u, &pin)));
    assert(pin == 0u);
    assert(bitset_status_ok(bitset_set(&pins, pin)));

    assert(bitset_status_ok(bitset_find_first_clear(&pins, 1u, &pin)));
    assert(pin == 1u);

    bitset_deinit(&pins);
}

static void test_bitset_logical_ops(void)
{
    enum { CAPACITY = 8u };

    static uint8_t left_storage[1];
    static uint8_t right_storage[1];
    static uint8_t tmp_storage[1];

    bitset_t left;
    bitset_t right;
    bitset_t tmp;

    assert(bitset_status_ok(bitset_init(&left, &(bitset_config_t){
        .capacity = CAPACITY,
        .storage = left_storage,
        .storage_bytes = sizeof left_storage,
    })));
    assert(bitset_status_ok(bitset_init(&right, &(bitset_config_t){
        .capacity = CAPACITY,
        .storage = right_storage,
        .storage_bytes = sizeof right_storage,
    })));
    assert(bitset_status_ok(bitset_init(&tmp, &(bitset_config_t){
        .capacity = CAPACITY,
        .storage = tmp_storage,
        .storage_bytes = sizeof tmp_storage,
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

    assert(bitset_status_ok(bitset_copy(&tmp, &left)));
    assert(bitset_status_ok(bitset_union_with(&tmp, &right)));
    assert(bitset_size(&tmp) == 4u);

    assert(bitset_status_ok(bitset_copy(&tmp, &left)));
    assert(bitset_status_ok(bitset_xor_with(&tmp, &right)));
    assert(bitset_test(&tmp, 1u));
    assert(bitset_test(&tmp, 3u));
    assert(!bitset_test(&tmp, 2u));

    assert(bitset_status_ok(bitset_complement(&tmp)));
    assert(bitset_test(&tmp, 0u));
    assert(bitset_test(&tmp, 2u));
    assert(bitset_test(&tmp, 4u));

    bitset_deinit(&left);
    bitset_deinit(&right);
    bitset_deinit(&tmp);
}

static void test_bitset_partial_byte(void)
{
    enum { CAPACITY = 10u };

    static uint8_t storage[(CAPACITY + 7u) / 8u];

    bitset_t bits;
    assert(bitset_status_ok(bitset_init(&bits, &(bitset_config_t){
        .capacity = CAPACITY,
        .storage = storage,
        .storage_bytes = sizeof storage,
    })));

    assert(bitset_status_ok(bitset_set_all(&bits)));
    assert(bitset_test(&bits, 9u));
    assert(!bitset_test(&bits, CAPACITY));

    assert(bitset_set(&bits, CAPACITY) == BITSET_ERR_INVALID);

    const uint8_t raw[] = { 0xFFu, 0x03u };
    assert(bitset_status_ok(bitset_load_bytes(&bits, raw, sizeof raw)));
    assert(bitset_size(&bits) == CAPACITY);

    uint8_t out[2] = {0};
    assert(bitset_status_ok(bitset_store_bytes(&bits, out, sizeof out)));
    assert(out[0] == 0xFFu);
    assert(out[1] == 0x03u);

    bitset_deinit(&bits);
}

static void test_bitset_arena(void)
{
    static uint8_t arena_backing[4096];

    arena_t arena;
    assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
        .backing = arena_backing,
        .backing_bytes = sizeof arena_backing,
    })));

    bitset_t *mask = NULL;
    assert(bitset_status_ok(bitset_create(&mask, 32u, &arena, BITSET_FLAG_NONE)));

    for (size_t i = 0u; i < 32u; i += 3u) {
        assert(bitset_status_ok(bitset_set(mask, i)));
    }

    size_t found = 0u;
    assert(bitset_status_ok(bitset_find_first_set(mask, 0u, &found)));
    assert(found == 0u);
    assert(bitset_status_ok(bitset_find_first_set(mask, 1u, &found)));
    assert(found == 3u);

    assert(bitset_data(mask) != NULL);
    assert(bitset_data_bytes(mask) == 4u);

    bitset_destroy(mask);
    arena_deinit(&arena);
}

#if RING_ALLOW_DYNAMIC_ALLOC
static void test_bitset_dynamic(void)
{
    bitset_t *gpio = NULL;
    assert(bitset_status_ok(bitset_create(&gpio, 28u, NULL, BITSET_FLAG_NONE)));

    size_t allocated[28] = {0};
    size_t count = 0u;

    for (size_t i = 0u; i < 10u; ++i) {
        size_t pin = 0u;
        assert(bitset_status_ok(bitset_find_first_clear(gpio, 0u, &pin)));
        assert(bitset_status_ok(bitset_set(gpio, pin)));
        allocated[count++] = pin;
    }

    assert(bitset_size(gpio) == 10u);

    for (size_t i = 0u; i < count; ++i) {
        assert(bitset_status_ok(bitset_reset(gpio, allocated[i])));
    }

    assert(bitset_empty(gpio));
    bitset_destroy(gpio);
}
#endif

int main(void)
{
    test_bitset_caller_owned();
    test_bitset_logical_ops();
    test_bitset_partial_byte();
    test_bitset_arena();
#if RING_ALLOW_DYNAMIC_ALLOC
    test_bitset_dynamic();
#endif

    puts("bitset: ok");
    return 0;
}
