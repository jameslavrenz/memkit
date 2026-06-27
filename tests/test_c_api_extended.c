#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "bitset.h"
#include "btree.h"
#include "deque.h"
#include "dlist.h"
#include "handle_pool.h"
#include "hashmap.h"
#include "list.h"
#include "lrucache.h"
#include "objpool.h"
#include "pqueue.h"
#include "queue.h"
#include "ring.h"
#include "stack.h"
#include "vector.h"

#if MEMKIT_C_API_EXTENDED

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
    arena_t *arena = NULL;
    assert(arena_status_ok(arena_create(&arena, 262144u)));

    {
        ring_t *created_ring = NULL;
        assert(ring_status_ok(ring_create(
            &created_ring,
            sizeof(uint32_t),
            4u,
            arena,
            0u
        )));
        const uint32_t created_value = 11u;
        assert(ring_status_ok(ring_push_back(created_ring, &created_value)));
        assert(ring_size(created_ring) == 1u);
        ring_destroy(created_ring);
    }

    {
        static uint8_t ring_storage[sizeof(uint32_t) * 4u];
        ring_t ring;
        assert(ring_status_ok(ring_init(&ring, &(ring_config_t){
            .elem_size = sizeof(uint32_t),
            .capacity = 4u,
            .storage = ring_storage,
            .storage_bytes = sizeof ring_storage,
        })));
        const uint32_t ring_value = 7u;
        assert(ring_status_ok(ring_push_back(&ring, &ring_value)));
        assert(ring_size(&ring) == 1u);
        ring_deinit(&ring);
    }

    {
        const uint32_t sample = 7u;
        queue_t queue;
        assert(queue_status_ok(queue_init(&queue, &(queue_config_t){
            .elem_size = sizeof(uint32_t),
            .capacity = 4u,
            .flags = QUEUE_FLAG_DYNAMIC_STORAGE | QUEUE_FLAG_GROWABLE,
        })));
        assert(queue_status_ok(queue_push(&queue, &sample)));
        assert(queue_size(&queue) == 1u);
        queue_deinit(&queue);
    }

    {
        cstack_t stack;
        assert(stack_status_ok(stack_init(&stack, &(stack_config_t){
            .elem_size = sizeof(int32_t),
            .capacity = 4u,
            .flags = STACK_FLAG_DYNAMIC_STORAGE | STACK_FLAG_GROWABLE,
        })));
        const int32_t stack_value = 55;
        assert(stack_status_ok(stack_push(&stack, &stack_value)));
        assert(stack_size(&stack) == 1u);
        stack_deinit(&stack);
    }

    {
        vector_t vec;
        assert(vector_status_ok(vector_init(&vec, &(vector_config_t){
            .elem_size = sizeof(int32_t),
            .capacity = 4u,
            .flags = VECTOR_FLAG_DYNAMIC_STORAGE | VECTOR_FLAG_GROWABLE,
        })));
        const int32_t vec_value = 99;
        assert(vector_status_ok(vector_push_back(&vec, &vec_value)));
        assert(vector_size(&vec) == 1u);
        vector_deinit(&vec);
    }

    {
        bitset_t bits;
        assert(bitset_status_ok(bitset_init(&bits, &(bitset_config_t){
            .capacity = 32u,
            .flags = BITSET_FLAG_DYNAMIC_STORAGE,
        })));
        assert(bitset_status_ok(bitset_set(&bits, 3u)));
        assert(bitset_test(&bits, 3u));
        assert(bitset_size(&bits) == 1u);
        bitset_deinit(&bits);
    }

    {
        objpool_t pool;
        assert(objpool_status_ok(objpool_init(&pool, &(objpool_config_t){
            .elem_size = sizeof(uint32_t),
            .capacity = 4u,
            .flags = OBJPOOL_FLAG_DYNAMIC_STORAGE,
        })));
        void *slot = NULL;
        assert(objpool_status_ok(objpool_alloc(&pool, &slot)));
        *(uint32_t *)slot = 123u;
        assert(objpool_size(&pool) == 1u);
        objpool_deinit(&pool);
    }

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
        void *handle_slot = NULL;
        handle_t handle = HANDLE_POOL_INVALID_HANDLE;
        assert(handle_pool_status_ok(handle_pool_acquire(&handles, &handle_slot, &handle)));
        *(uint32_t *)handle_slot = 900u;
        assert(handle_pool_valid(&handles, handle));
        handle_pool_deinit(&handles);
    }

    {
        handle_pool_t *arena_pool = NULL;
        assert(handle_pool_status_ok(handle_pool_create(
            &arena_pool,
            sizeof(int32_t),
            4u,
            arena,
            0u
        )));
        void *arena_slot = NULL;
        handle_t arena_handle = HANDLE_POOL_INVALID_HANDLE;
        assert(handle_pool_status_ok(handle_pool_acquire(arena_pool, &arena_slot, &arena_handle)));
        *(int32_t *)arena_slot = -7;
        handle_pool_destroy(arena_pool);
    }

    {
        const uint32_t sample = 7u;

        queue_t *created_queue = NULL;
        assert(queue_status_ok(queue_create(
            &created_queue, sizeof(uint32_t), 4u, arena, 0u)));
        assert(queue_status_ok(queue_push(created_queue, &sample)));
        queue_destroy(created_queue);

        cstack_t *created_stack = NULL;
        assert(stack_status_ok(stack_create(
            &created_stack, sizeof(int32_t), 4u, arena, 0u)));
        const int32_t stack_value = 55;
        assert(stack_status_ok(stack_push(created_stack, &stack_value)));
        stack_destroy(created_stack);

        vector_t *created_vector = NULL;
        assert(vector_status_ok(vector_create(
            &created_vector, sizeof(int32_t), 4u, arena, 0u)));
        const int32_t vec_value = 99;
        assert(vector_status_ok(vector_push_back(created_vector, &vec_value)));
        vector_destroy(created_vector);

        bitset_t *created_bitset = NULL;
        assert(bitset_status_ok(bitset_create(&created_bitset, 32u, arena, 0u)));
        assert(bitset_status_ok(bitset_set(created_bitset, 3u)));
        bitset_destroy(created_bitset);

        objpool_t *created_pool = NULL;
        assert(objpool_status_ok(objpool_create(
            &created_pool, sizeof(uint32_t), 4u, arena, 0u)));
        void *pool_slot = NULL;
        assert(objpool_status_ok(objpool_alloc(created_pool, &pool_slot)));
        objpool_destroy(created_pool);

        hashmap_t *created_map = NULL;
        assert(hashmap_status_ok(hashmap_create(
            &created_map,
            sizeof(uint32_t),
            sizeof(int32_t),
            8u,
            HASHMAP_STRATEGY_CHAINING,
            arena,
            HASHMAP_FLAG_GROWABLE
        )));
        const int32_t mapped = 42;
        assert(hashmap_status_ok(hashmap_put(created_map, &sample, &mapped)));
        hashmap_destroy(created_map);

        list_t *created_list = NULL;
        assert(list_status_ok(list_create(
            &created_list, sizeof(uint32_t), arena, 0u)));
        assert(list_status_ok(list_push_back(created_list, &sample)));
        list_destroy(created_list);

        dlist_t *created_dlist = NULL;
        assert(dlist_status_ok(dlist_create(
            &created_dlist, sizeof(uint32_t), arena, 0u)));
        assert(dlist_status_ok(dlist_push_back(created_dlist, &sample)));
        dlist_destroy(created_dlist);

        deque_t *created_deque = NULL;
        assert(deque_status_ok(deque_create(
            &created_deque, sizeof(uint32_t), 4u, arena, 0u)));
        assert(deque_status_ok(deque_push_back(created_deque, &sample)));
        deque_destroy(created_deque);

        pqueue_t *created_pqueue = NULL;
        assert(pqueue_status_ok(pqueue_create(
            &created_pqueue,
            sizeof(uint32_t),
            compare_u32,
            8u,
            arena,
            PQUEUE_FLAG_GROWABLE
        )));
        const uint32_t pq_values[] = {5u, 1u, 9u, 3u};
        for (size_t i = 0u; i < 4u; ++i) {
            assert(pqueue_status_ok(pqueue_push(created_pqueue, &pq_values[i])));
        }
        uint32_t pq_top = 0u;
        assert(pqueue_status_ok(pqueue_peek(created_pqueue, &pq_top)));
        assert(pq_top == 1u);
        pqueue_destroy(created_pqueue);

        btree_t *created_tree = NULL;
        assert(btree_status_ok(btree_create(
            &created_tree,
            sizeof(uint32_t),
            compare_u32,
            arena,
            0u
        )));
        assert(btree_status_ok(btree_insert(created_tree, &sample)));
        btree_destroy(created_tree);

        lrucache_t *created_cache = NULL;
        assert(lrucache_status_ok(lrucache_create(
            &created_cache,
            sizeof(uint32_t),
            sizeof(int32_t),
            4u,
            0u,
            arena,
            0u
        )));
        const int32_t cached_value = 3;
        assert(lrucache_status_ok(lrucache_put(created_cache, &sample, &cached_value)));
        lrucache_destroy(created_cache);
    }

    {
        hashmap_t map;
        assert(hashmap_status_ok(hashmap_init(&map, &(hashmap_config_t){
            .key_size     = sizeof(uint32_t),
            .value_size   = sizeof(int32_t),
            .bucket_count = 8u,
            .strategy     = HASHMAP_STRATEGY_CHAINING,
            .flags        = HASHMAP_FLAG_GROWABLE | HASHMAP_FLAG_DYNAMIC_STORAGE,
        })));
        for (uint32_t i = 0u; i < 16u; ++i) {
            const int32_t value = (int32_t)(i * 10);
            assert(hashmap_status_ok(hashmap_put(&map, &i, &value)));
        }
        assert(hashmap_size(&map) == 16u);
        int32_t got = 0;
        const uint32_t key = 5u;
        assert(hashmap_status_ok(hashmap_get(&map, &key, &got)));
        assert(got == 50);
        hashmap_deinit(&map);
    }

    {
        const uint32_t list_value = 42u;
        list_t list;
        assert(list_status_ok(list_init(&list, &(list_config_t){
            .elem_size = sizeof(uint32_t),
            .flags     = LIST_FLAG_DYNAMIC_STORAGE,
        })));
        assert(list_status_ok(list_push_back(&list, &list_value)));
        assert(list_size(&list) == 1u);
        list_deinit(&list);
    }

    {
        const uint32_t list_value = 42u;
        dlist_t dlist;
        assert(dlist_status_ok(dlist_init(&dlist, &(dlist_config_t){
            .elem_size = sizeof(uint32_t),
            .flags     = DLIST_FLAG_DYNAMIC_STORAGE,
        })));
        assert(dlist_status_ok(dlist_push_back(&dlist, &list_value)));
        assert(dlist_size(&dlist) == 1u);
        dlist_deinit(&dlist);
    }

    {
        const uint32_t list_value = 42u;
        deque_t deque;
        assert(deque_status_ok(deque_init(&deque, &(deque_config_t){
            .elem_size = sizeof(uint32_t),
            .capacity = 4u,
            .flags = DEQUE_FLAG_DYNAMIC_STORAGE | DEQUE_FLAG_GROWABLE,
        })));
        assert(deque_status_ok(deque_push_back(&deque, &list_value)));
        assert(deque_size(&deque) == 1u);
        deque_deinit(&deque);
    }

    {
        pqueue_t pqueue;
        assert(pqueue_status_ok(pqueue_init(&pqueue, &(pqueue_config_t){
            .elem_size = sizeof(uint32_t),
            .capacity = 8u,
            .compare_fn = compare_u32,
            .flags = PQUEUE_FLAG_DYNAMIC_STORAGE | PQUEUE_FLAG_GROWABLE,
        })));
        const uint32_t pq_values[] = {5u, 1u, 9u, 3u};
        for (size_t i = 0u; i < 4u; ++i) {
            assert(pqueue_status_ok(pqueue_push(&pqueue, &pq_values[i])));
        }
        uint32_t top = 0u;
        assert(pqueue_status_ok(pqueue_peek(&pqueue, &top)));
        assert(top == 1u);
        pqueue_deinit(&pqueue);
    }

    {
        btree_t tree;
        typedef struct {
            uint32_t key;
            int32_t value;
        } btree_kv_t;
        assert(btree_status_ok(btree_init(&tree, &(btree_config_t){
            .elem_size     = sizeof(btree_kv_t),
            .node_capacity = 8u,
            .flags         = BTREE_FLAG_DYNAMIC_STORAGE,
        })));
        for (uint32_t i = 0u; i < 4u; ++i) {
            const btree_kv_t kv = {i, (int32_t)i};
            assert(btree_status_ok(btree_insert(&tree, &kv)));
        }
        assert(btree_size(&tree) == 4u);
        btree_deinit(&tree);
    }

    {
        lrucache_t cache;
        assert(lrucache_status_ok(lrucache_init(&cache, &(lrucache_config_t){
            .key_size   = sizeof(uint32_t),
            .value_size = sizeof(int32_t),
            .capacity   = 4u,
            .flags      = LRUCACHE_FLAG_DYNAMIC_STORAGE,
        })));
        for (uint32_t i = 0u; i < 4u; ++i) {
            const int32_t value = (int32_t)i;
            assert(lrucache_status_ok(lrucache_put(&cache, &i, &value)));
        }
        assert(lrucache_size(&cache) == 4u);
        int32_t cached = 0;
        const uint32_t cache_key = 3u;
        assert(lrucache_status_ok(lrucache_get(&cache, &cache_key, &cached)));
        assert(cached == 3);
        lrucache_deinit(&cache);
    }

    arena_destroy(arena);
    return 0;
}

#else

int main(void)
{
    return 0;
}

#endif
