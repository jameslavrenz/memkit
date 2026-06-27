#include <assert.h>
#include <stdint.h>

#include "arena.h"
#include "handle_pool.h"
#include "hashmap.h"
#include "ring.h"

int main(void)
{
    static uint8_t ring_storage[sizeof(uint32_t) * 4u];

    ring_t ring;
    assert(ring_status_ok(ring_init(&ring, &(ring_config_t){
        .elem_size = sizeof(uint32_t),
        .capacity = 4u,
        .storage = ring_storage,
        .storage_bytes = sizeof ring_storage,
    })));

    const uint32_t value = 42u;
    assert(ring_status_ok(ring_push_back(&ring, &value)));
    assert(ring_size(&ring) == 1u);

    ring_deinit(&ring);

    {
        static uint8_t handle_storage[sizeof(uint32_t) * 4u];
        static uint16_t handle_generations[4u];
        static uint32_t handle_free_stack[4u];

        handle_pool_t handles;
        assert(handle_pool_status_ok(handle_pool_init(&handles, &(handle_pool_config_t){
            .elem_size = sizeof(uint32_t),
            .capacity = 4u,
            .storage = handle_storage,
            .storage_bytes = sizeof handle_storage,
            .generations = handle_generations,
            .generations_bytes = sizeof handle_generations,
            .free_stack = handle_free_stack,
            .free_stack_bytes = sizeof handle_free_stack,
        })));

        void *slot = NULL;
        handle_t handle = HANDLE_POOL_INVALID_HANDLE;
        assert(handle_pool_status_ok(handle_pool_acquire(&handles, &slot, &handle)));
        assert(handle_pool_valid(&handles, handle));
        handle_pool_deinit(&handles);
    }

    {
        static uint8_t arena_backing[2048u];

        arena_t arena;
        assert(arena_status_ok(arena_init(&arena, &(arena_config_t){
            .backing = arena_backing,
            .backing_bytes = sizeof arena_backing,
        })));

        ring_t *created_ring = NULL;
        assert(ring_status_ok(ring_create(
            &created_ring,
            sizeof(uint32_t),
            4u,
            &arena,
            0u
        )));
        assert(ring_status_ok(ring_push_back(created_ring, &value)));
        assert(ring_size(created_ring) == 1u);
        ring_destroy(created_ring);

        handle_pool_t *created_pool = NULL;
        assert(handle_pool_status_ok(handle_pool_create(
            &created_pool,
            sizeof(uint32_t),
            4u,
            &arena,
            0u
        )));
        void *arena_slot = NULL;
        handle_t arena_handle = HANDLE_POOL_INVALID_HANDLE;
        assert(handle_pool_status_ok(handle_pool_acquire(
            created_pool,
            &arena_slot,
            &arena_handle
        )));
        handle_pool_destroy(created_pool);

        arena_deinit(&arena);
    }

#if !MEMKIT_C_API_EXTENDED
    hashmap_t map;
    assert(hashmap_init(&map, &(hashmap_config_t){
        .key_size = sizeof(uint32_t),
        .value_size = sizeof(uint32_t),
        .bucket_count = 4u,
    }) == HASHMAP_ERR_UNSUPPORTED);
#endif

    return 0;
}
