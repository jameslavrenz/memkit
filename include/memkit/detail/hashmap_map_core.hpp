#pragma once

#include "../config.hpp"
#include "../status.hpp"
#include "element_policy.hpp"
#include "hash_policy.hpp"
#include "utility.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

namespace memkit::detail {

enum class hashmap_strategy : std::uint8_t {
    chaining        = 0,
    open_addressing = 1,
};

enum class hashmap_policy : unsigned {
    none     = 0u,
    growable = 1u << 0u,
};

enum class hashmap_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline hashmap_storage_kind operator|(
    hashmap_storage_kind a,
    hashmap_storage_kind b
) noexcept
{
    return static_cast<hashmap_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

enum class hashmap_slot_state : std::uint8_t {
    empty    = 0,
    deleted  = 1,
    occupied = 2,
};

struct hashmap_chain_node {
    hashmap_chain_node* next = nullptr;
};

class hashmap_map_core {
public:
    using raw_alloc_fn = status (*)(void*, std::size_t, std::size_t, void**);

    hashmap_map_core() = default;

    [[nodiscard]] static std::size_t open_slot_stride(
        std::size_t key_size,
        std::size_t value_size
    ) noexcept
    {
        const std::size_t key_alignment = bytes_alignment(key_size);
        const std::size_t key_offset    = align_up(1u, key_alignment);
        const std::size_t value_offset  = key_offset + key_size;
        return value_offset + value_size;
    }

    [[nodiscard]] static std::size_t chain_node_bytes(
        std::size_t key_size,
        std::size_t value_size
    ) noexcept
    {
        return sizeof(hashmap_chain_node) + key_size + value_size;
    }

    void reset_state() noexcept
    {
        strategy_     = hashmap_strategy::chaining;
        bucket_count_ = 0u;
        size_         = 0u;
        key_offset_   = 0u;
        value_offset_ = 0u;
        slot_stride_  = 0u;
        buckets_      = nullptr;
        slots_        = nullptr;
        key_policy_   = runtime_element_policy{};
        value_policy_ = runtime_element_policy{};
        hash_policy_  = runtime_hash_key_policy{};
        policy_       = hashmap_policy::none;
        arena_ctx_      = nullptr;
        arena_alloc_fn_ = nullptr;
        storage_kind_   = hashmap_storage_kind::external;
    }

    [[nodiscard]] status init_chaining(
        hashmap_chain_node** buckets,
        std::size_t bucket_count,
        runtime_element_policy key_policy,
        runtime_element_policy value_policy,
        runtime_hash_key_policy hash_policy,
        hashmap_policy policy = hashmap_policy::none
    ) noexcept
    {
        if (buckets == nullptr || bucket_count == 0u) {
            return status::invalid;
        }

        reset_state();
        strategy_     = hashmap_strategy::chaining;
        buckets_      = buckets;
        bucket_count_ = bucket_count;
        key_policy_   = key_policy;
        value_policy_ = value_policy;
        hash_policy_  = hash_policy;
        policy_       = policy;

        for (std::size_t i = 0u; i < bucket_count_; ++i) {
            buckets_[i] = nullptr;
        }

        return status::ok;
    }

    [[nodiscard]] status init_open(
        std::byte* slots,
        std::size_t bucket_count,
        std::size_t key_offset,
        std::size_t value_offset,
        std::size_t slot_stride,
        runtime_element_policy key_policy,
        runtime_element_policy value_policy,
        runtime_hash_key_policy hash_policy,
        hashmap_policy policy = hashmap_policy::none
    ) noexcept
    {
        if (slots == nullptr || bucket_count == 0u || slot_stride == 0u) {
            return status::invalid;
        }

        reset_state();
        strategy_     = hashmap_strategy::open_addressing;
        slots_        = slots;
        bucket_count_ = bucket_count;
        key_offset_   = key_offset;
        value_offset_ = value_offset;
        slot_stride_  = slot_stride;
        key_policy_   = key_policy;
        value_policy_ = value_policy;
        hash_policy_  = hash_policy;
        policy_       = policy;

        for (std::size_t i = 0u; i < bucket_count_; ++i) {
            open_slot_at(i)[0] = static_cast<std::byte>(hashmap_slot_state::empty);
        }

        return status::ok;
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t initial_buckets,
        hashmap_strategy strategy,
        runtime_element_policy key_policy,
        runtime_element_policy value_policy,
        runtime_hash_key_policy hash_policy,
        hashmap_policy policy = hashmap_policy::growable
    )
    {
        if (initial_buckets == 0u) {
            initial_buckets = default_buckets;
        }

        reset_state();
        strategy_     = strategy;
        key_policy_   = key_policy;
        value_policy_ = value_policy;
        hash_policy_  = hash_policy;
        policy_       = policy;
        size_         = 0u;
        bind_arena(arena);

        if (strategy_ == hashmap_strategy::chaining) {
            void* ptr = nullptr;
            const std::size_t bytes = initial_buckets * sizeof(hashmap_chain_node*);
            const status st = alloc_raw(bytes, alignof(hashmap_chain_node*), &ptr);
            if (!ok(st)) {
                unbind_arena();
                return st;
            }

            buckets_      = static_cast<hashmap_chain_node**>(ptr);
            bucket_count_ = initial_buckets;
            set_storage_kind(hashmap_storage_kind::owns | hashmap_storage_kind::arena);

            for (std::size_t i = 0u; i < bucket_count_; ++i) {
                buckets_[i] = nullptr;
            }
            return status::ok;
        }

        key_offset_  = align_up(1u, key_policy_.alignment());
        value_offset_ = key_offset_ + key_policy_.elem_size();
        slot_stride_  = value_offset_ + value_policy_.elem_size();

        void* ptr = nullptr;
        const std::size_t bytes = initial_buckets * slot_stride_;
        const status st = alloc_raw(bytes, key_offset_, &ptr);
        if (!ok(st)) {
            unbind_arena();
            return st;
        }

        slots_        = static_cast<std::byte*>(ptr);
        bucket_count_ = initial_buckets;
        set_storage_kind(hashmap_storage_kind::owns | hashmap_storage_kind::arena);

        std::memset(slots_, 0, bytes);
        for (std::size_t i = 0u; i < bucket_count_; ++i) {
            open_slot_at(i)[0] = static_cast<std::byte>(hashmap_slot_state::empty);
        }
        return status::ok;
    }

    [[nodiscard]] hashmap_strategy strategy() const noexcept { return strategy_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t bucket_count() const noexcept { return bucket_count_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }
    [[nodiscard]] std::size_t key_offset() const noexcept { return key_offset_; }
    [[nodiscard]] std::size_t value_offset() const noexcept { return value_offset_; }
    [[nodiscard]] std::size_t slot_stride() const noexcept { return slot_stride_; }
    [[nodiscard]] hashmap_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] hashmap_chain_node** buckets() const noexcept { return buckets_; }
    [[nodiscard]] std::byte* slots() const noexcept { return slots_; }
    [[nodiscard]] void* arena_ctx() const noexcept { return arena_ctx_; }

    void set_storage_kind(hashmap_storage_kind kind) noexcept { storage_kind_ = kind; }

    void bind_allocator(void* ctx, raw_alloc_fn fn) noexcept
    {
        arena_ctx_      = ctx;
        arena_alloc_fn_ = fn;
    }

    void clear() noexcept
    {
        if (bucket_count_ == 0u) {
            size_ = 0u;
            return;
        }

        if (strategy_ == hashmap_strategy::chaining) {
            chain_clear();
        } else {
            open_clear();
        }

        size_ = 0u;
    }

    [[nodiscard]] status put(const void* key, const void* value)
    {
        if (bucket_count_ == 0u || key == nullptr || value == nullptr) {
            return status::null_ptr;
        }

        if (should_grow()) {
            const status grow_st = maybe_grow();
            if (!ok(grow_st)) {
                return grow_st;
            }
        }

        if (strategy_ == hashmap_strategy::chaining) {
            return chain_put(key, value);
        }
        return open_put(key, value);
    }

    [[nodiscard]] status get(const void* key, void* out_value) const
    {
        if (key == nullptr || out_value == nullptr) {
            return status::null_ptr;
        }

        if (strategy_ == hashmap_strategy::chaining) {
            return chain_get(key, out_value);
        }
        return open_get(key, out_value);
    }

    [[nodiscard]] status remove(const void* key)
    {
        if (key == nullptr) {
            return status::null_ptr;
        }

        if (strategy_ == hashmap_strategy::chaining) {
            return chain_remove(key);
        }
        return open_remove(key);
    }

    [[nodiscard]] bool contains(const void* key) const
    {
        if (key == nullptr) {
            return false;
        }

        if (strategy_ == hashmap_strategy::chaining) {
            return chain_contains(key);
        }
        return open_contains(key);
    }

    template<typename Visitor>
    [[nodiscard]] status foreach(Visitor&& visit) const
    {
        if (strategy_ == hashmap_strategy::chaining) {
            for (std::size_t i = 0u; i < bucket_count_; ++i) {
                for (hashmap_chain_node* node = buckets_[i]; node != nullptr; node = node->next) {
                    const status st = visit(
                        chain_node_key(node),
                        chain_node_value(node)
                    );
                    if (!ok(st)) {
                        return st;
                    }
                }
            }
            return status::ok;
        }

        for (std::size_t i = 0u; i < bucket_count_; ++i) {
            const std::byte* slot = open_slot_at(i);
            if (slot[0] != static_cast<std::byte>(hashmap_slot_state::occupied)) {
                continue;
            }

            const status st = visit(
                const_cast<void*>(static_cast<const void*>(slot + key_offset_)),
                const_cast<void*>(static_cast<const void*>(slot + value_offset_))
            );
            if (!ok(st)) {
                return st;
            }
        }
        return status::ok;
    }

    void release_storage() noexcept
    {
        if ((static_cast<std::uint8_t>(storage_kind_) &
             static_cast<std::uint8_t>(hashmap_storage_kind::owns)) != 0u &&
            (static_cast<std::uint8_t>(storage_kind_) &
             static_cast<std::uint8_t>(hashmap_storage_kind::heap)) != 0u) {
            if (strategy_ == hashmap_strategy::chaining && buckets_ != nullptr) {
                free_raw(buckets_);
            } else if (strategy_ == hashmap_strategy::open_addressing && slots_ != nullptr) {
                free_raw(slots_);
            }
        }

        buckets_      = nullptr;
        slots_        = nullptr;
        bucket_count_ = 0u;
        storage_kind_ = hashmap_storage_kind::external;
        unbind_arena();
    }

    [[nodiscard]] status alloc_raw(std::size_t bytes, std::size_t alignment, void** out_ptr)
    {
        if (arena_alloc_fn_ != nullptr) {
            return arena_alloc_fn_(arena_ctx_, bytes, alignment, out_ptr);
        }

#if MEMKIT_ALLOW_HEAP
        void* const ptr = std::malloc(bytes);
        if (ptr == nullptr) {
            return status::oom;
        }
        *out_ptr = ptr;
        return status::ok;
#else
        (void)bytes;
        (void)alignment;
        (void)out_ptr;
        return status::oom;
#endif
    }

private:
    static constexpr std::size_t default_buckets = 8u;
    static constexpr std::size_t load_num        = 3u;
    static constexpr std::size_t load_den        = 4u;

    template<typename Arena>
    static status arena_alloc_trampoline(void* ctx, std::size_t size, std::size_t align, void** out)
    {
        return static_cast<Arena*>(ctx)->allocate(size, align, out);
    }

    template<typename Arena>
    void bind_arena(Arena& arena) noexcept
    {
        arena_ctx_      = &arena;
        arena_alloc_fn_ = &arena_alloc_trampoline<Arena>;
    }

    void unbind_arena() noexcept
    {
        arena_ctx_      = nullptr;
        arena_alloc_fn_ = nullptr;
    }

    void free_raw(void* ptr) noexcept
    {
        if (ptr == nullptr || arena_alloc_fn_ != nullptr) {
            return;
        }

#if MEMKIT_ALLOW_HEAP
        std::free(ptr);
#else
        (void)ptr;
#endif
    }

    void free_chain_node(hashmap_chain_node* node) noexcept
    {
        free_raw(node);
    }

    [[nodiscard]] void* chain_node_key(hashmap_chain_node* node) const noexcept
    {
        return reinterpret_cast<std::byte*>(node) + sizeof(hashmap_chain_node);
    }

    [[nodiscard]] void* chain_node_value(hashmap_chain_node* node) const noexcept
    {
        return reinterpret_cast<std::byte*>(node) + sizeof(hashmap_chain_node) + key_policy_.elem_size();
    }

    [[nodiscard]] const void* chain_node_key_const(const hashmap_chain_node* node) const noexcept
    {
        return reinterpret_cast<const std::byte*>(node) + sizeof(hashmap_chain_node);
    }

    [[nodiscard]] const void* chain_node_value_const(const hashmap_chain_node* node) const noexcept
    {
        return reinterpret_cast<const std::byte*>(node) + sizeof(hashmap_chain_node) +
               key_policy_.elem_size();
    }

    [[nodiscard]] std::byte* open_slot_at(std::size_t index) noexcept
    {
        return slots_ + (index * slot_stride_);
    }

    [[nodiscard]] const std::byte* open_slot_at(std::size_t index) const noexcept
    {
        return slots_ + (index * slot_stride_);
    }

    [[nodiscard]] bool should_grow() const noexcept
    {
        if ((static_cast<unsigned>(policy_) &
             static_cast<unsigned>(hashmap_policy::growable)) == 0u) {
            return false;
        }
        return (size_ + 1u) * load_den >= bucket_count_ * load_num;
    }

    [[nodiscard]] std::size_t grow_bucket_count(std::size_t current) const noexcept
    {
        if (current == 0u) {
            return default_buckets;
        }
        if (current > SIZE_MAX / 2u) {
            return current;
        }
        return current * 2u;
    }

    [[nodiscard]] status maybe_grow()
    {
        const std::size_t new_count = grow_bucket_count(bucket_count_);
        if (new_count == bucket_count_) {
            return status::full;
        }

        if (strategy_ == hashmap_strategy::chaining) {
            return chain_rehash(new_count);
        }
        return open_rehash(new_count);
    }

    [[nodiscard]] status alloc_chain_node(
        const void* key,
        const void* value,
        hashmap_chain_node** out_node
    )
    {
        void* memory = nullptr;
        const std::size_t bytes = chain_node_bytes(key_policy_.elem_size(), value_policy_.elem_size());
        const status st = alloc_raw(bytes, alignof(hashmap_chain_node), &memory);
        if (!ok(st)) {
            return st;
        }

        auto* node = static_cast<hashmap_chain_node*>(memory);
        node->next = nullptr;

        const status key_st = key_policy_.copy_construct(chain_node_key(node), key);
        if (!ok(key_st)) {
            free_chain_node(node);
            return key_st;
        }

        const status value_st =
            value_policy_.copy_construct(chain_node_value(node), value);
        if (!ok(value_st)) {
            key_policy_.destroy(chain_node_key(node));
            free_chain_node(node);
            return value_st;
        }

        *out_node = node;
        return status::ok;
    }

    void destroy_chain_node(hashmap_chain_node* node) noexcept
    {
        if (node == nullptr) {
            return;
        }
        key_policy_.destroy(chain_node_key(node));
        value_policy_.destroy(chain_node_value(node));
    }

    [[nodiscard]] hashmap_chain_node* chain_find(
        std::size_t bucket_index,
        const void* key,
        hashmap_chain_node** out_prev
    ) const
    {
        hashmap_chain_node* prev = nullptr;
        hashmap_chain_node* node = buckets_[bucket_index];

        while (node != nullptr) {
            if (hash_policy_.equal(chain_node_key_const(node), key)) {
                if (out_prev != nullptr) {
                    *out_prev = prev;
                }
                return node;
            }
            prev = node;
            node = node->next;
        }

        if (out_prev != nullptr) {
            *out_prev = prev;
        }
        return nullptr;
    }

    [[nodiscard]] status chain_put(const void* key, const void* value)
    {
        const std::size_t bucket_index = hash_policy_.hash(key) % bucket_count_;
        hashmap_chain_node* existing   = chain_find(bucket_index, key, nullptr);

        if (existing != nullptr) {
            value_policy_.copy_assign(chain_node_value(existing), value);
            return status::ok;
        }

        hashmap_chain_node* node = nullptr;
        const status st = alloc_chain_node(key, value, &node);
        if (!ok(st)) {
            return st;
        }

        node->next           = buckets_[bucket_index];
        buckets_[bucket_index] = node;
        ++size_;
        return status::ok;
    }

    [[nodiscard]] status chain_get(const void* key, void* out_value) const
    {
        const std::size_t bucket_index = hash_policy_.hash(key) % bucket_count_;
        hashmap_chain_node* node       = buckets_[bucket_index];

        while (node != nullptr) {
            if (hash_policy_.equal(chain_node_key_const(node), key)) {
                const_cast<runtime_element_policy&>(value_policy_).copy_assign(
                    out_value,
                    chain_node_value_const(node)
                );
                return status::ok;
            }
            node = node->next;
        }

        return status::not_found;
    }

    [[nodiscard]] status chain_remove(const void* key)
    {
        const std::size_t bucket_index = hash_policy_.hash(key) % bucket_count_;
        hashmap_chain_node* prev       = nullptr;
        hashmap_chain_node* node       = chain_find(bucket_index, key, &prev);

        if (node == nullptr) {
            return status::not_found;
        }

        if (prev == nullptr) {
            buckets_[bucket_index] = node->next;
        } else {
            prev->next = node->next;
        }

        destroy_chain_node(node);
        free_chain_node(node);
        --size_;
        return status::ok;
    }

    [[nodiscard]] bool chain_contains(const void* key) const
    {
        const std::size_t bucket_index = hash_policy_.hash(key) % bucket_count_;
        return chain_find(bucket_index, key, nullptr) != nullptr;
    }

    void chain_clear() noexcept
    {
        for (std::size_t i = 0u; i < bucket_count_; ++i) {
            hashmap_chain_node* node = buckets_[i];
            while (node != nullptr) {
                hashmap_chain_node* const next = node->next;
                destroy_chain_node(node);
                free_chain_node(node);
                node = next;
            }
            buckets_[i] = nullptr;
        }
    }

    [[nodiscard]] status chain_rehash(std::size_t new_bucket_count)
    {
        void* ptr = nullptr;
        const std::size_t bytes = new_bucket_count * sizeof(hashmap_chain_node*);
        const status alloc_st = alloc_raw(bytes, alignof(hashmap_chain_node*), &ptr);
        if (!ok(alloc_st)) {
            return alloc_st;
        }

        auto* new_buckets = static_cast<hashmap_chain_node**>(ptr);
        std::memset(new_buckets, 0, bytes);

        for (std::size_t i = 0u; i < bucket_count_; ++i) {
            hashmap_chain_node* node = buckets_[i];
            while (node != nullptr) {
                hashmap_chain_node* const next = node->next;
                const std::size_t index =
                    hash_policy_.hash(chain_node_key_const(node)) % new_bucket_count;
                node->next        = new_buckets[index];
                new_buckets[index] = node;
                node              = next;
            }
        }

        if ((static_cast<std::uint8_t>(storage_kind_) &
             static_cast<std::uint8_t>(hashmap_storage_kind::owns)) != 0u &&
            (static_cast<std::uint8_t>(storage_kind_) &
             static_cast<std::uint8_t>(hashmap_storage_kind::heap)) != 0u &&
            buckets_ != nullptr) {
            free_raw(buckets_);
        }

        buckets_      = new_buckets;
        bucket_count_ = new_bucket_count;
        if (arena_alloc_fn_ != nullptr) {
            set_storage_kind(hashmap_storage_kind::owns | hashmap_storage_kind::arena);
        } else {
            set_storage_kind(hashmap_storage_kind::owns | hashmap_storage_kind::heap);
        }
        return status::ok;
    }

    [[nodiscard]] hashmap_slot_state open_probe(
        const void* key,
        std::size_t& out_index,
        bool for_insert
    ) const
    {
        std::size_t index         = hash_policy_.hash(key) % bucket_count_;
        std::size_t first_deleted = SIZE_MAX;

        for (std::size_t probes = 0u; probes < bucket_count_; ++probes) {
            const std::byte* slot = open_slot_at(index);
            const auto state = static_cast<hashmap_slot_state>(slot[0]);

            if (state == hashmap_slot_state::empty) {
                out_index = (for_insert && first_deleted != SIZE_MAX) ? first_deleted : index;
                return state;
            }

            if (state == hashmap_slot_state::deleted) {
                if (for_insert && first_deleted == SIZE_MAX) {
                    first_deleted = index;
                }
            } else if (hash_policy_.equal(slot + key_offset_, key)) {
                out_index = index;
                return state;
            }

            index = (index + 1u) % bucket_count_;
        }

        out_index = (for_insert && first_deleted != SIZE_MAX) ? first_deleted : index;
        return hashmap_slot_state::occupied;
    }

    [[nodiscard]] status open_put(const void* key, const void* value)
    {
        std::size_t index = 0u;
        const hashmap_slot_state state = open_probe(key, index, true);
        std::byte* slot                = open_slot_at(index);

        if (state == hashmap_slot_state::occupied) {
            if (hash_policy_.equal(slot + key_offset_, key)) {
                value_policy_.copy_assign(slot + value_offset_, value);
                return status::ok;
            }
            return status::full;
        }

        const status key_st = key_policy_.copy_construct(slot + key_offset_, key);
        if (!ok(key_st)) {
            return key_st;
        }

        const status value_st = value_policy_.copy_construct(slot + value_offset_, value);
        if (!ok(value_st)) {
            key_policy_.destroy(slot + key_offset_);
            slot[0] = static_cast<std::byte>(hashmap_slot_state::empty);
            return value_st;
        }

        slot[0] = static_cast<std::byte>(hashmap_slot_state::occupied);
        ++size_;
        return status::ok;
    }

    [[nodiscard]] status open_get(const void* key, void* out_value) const
    {
        std::size_t index = 0u;
        const hashmap_slot_state state = open_probe(key, index, false);
        if (state != hashmap_slot_state::occupied) {
            return status::not_found;
        }

        const std::byte* slot = open_slot_at(index);
        const_cast<runtime_element_policy&>(value_policy_).copy_assign(
            out_value,
            slot + value_offset_
        );
        return status::ok;
    }

    [[nodiscard]] status open_remove(const void* key)
    {
        std::size_t index = 0u;
        const hashmap_slot_state state = open_probe(key, index, false);
        if (state != hashmap_slot_state::occupied) {
            return status::not_found;
        }

        std::byte* slot = open_slot_at(index);
        key_policy_.destroy(slot + key_offset_);
        value_policy_.destroy(slot + value_offset_);
        slot[0] = static_cast<std::byte>(hashmap_slot_state::deleted);
        --size_;
        return status::ok;
    }

    [[nodiscard]] bool open_contains(const void* key) const
    {
        std::size_t index = 0u;
        return open_probe(key, index, false) == hashmap_slot_state::occupied;
    }

    void open_clear() noexcept
    {
        for (std::size_t i = 0u; i < bucket_count_; ++i) {
            std::byte* slot = open_slot_at(i);
            if (slot[0] == static_cast<std::byte>(hashmap_slot_state::occupied)) {
                key_policy_.destroy(slot + key_offset_);
                value_policy_.destroy(slot + value_offset_);
            }
            slot[0] = static_cast<std::byte>(hashmap_slot_state::empty);
        }
    }

    [[nodiscard]] status open_rehash(std::size_t new_bucket_count)
    {
        std::byte* const old_slots        = slots_;
        const std::size_t old_bucket_count = bucket_count_;
        const std::size_t old_size         = size_;

        void* ptr = nullptr;
        const std::size_t bytes = new_bucket_count * slot_stride_;
        const status alloc_st = alloc_raw(bytes, key_offset_, &ptr);
        if (!ok(alloc_st)) {
            return alloc_st;
        }

        auto* new_slots = static_cast<std::byte*>(ptr);
        std::memset(new_slots, 0, bytes);
        for (std::size_t i = 0u; i < new_bucket_count; ++i) {
            new_slots[i * slot_stride_] = static_cast<std::byte>(hashmap_slot_state::empty);
        }

        slots_        = new_slots;
        bucket_count_ = new_bucket_count;
        size_         = 0u;

        for (std::size_t i = 0u; i < old_bucket_count; ++i) {
            const std::byte* old_slot = old_slots + (i * slot_stride_);
            if (old_slot[0] != static_cast<std::byte>(hashmap_slot_state::occupied)) {
                continue;
            }

            const status st = open_put(
                old_slot + key_offset_,
                old_slot + value_offset_
            );
            if (!ok(st)) {
                slots_        = old_slots;
                bucket_count_ = old_bucket_count;
                size_         = old_size;
                free_raw(new_slots);
                return st;
            }
        }

        if ((static_cast<std::uint8_t>(storage_kind_) &
             static_cast<std::uint8_t>(hashmap_storage_kind::owns)) != 0u &&
            (static_cast<std::uint8_t>(storage_kind_) &
             static_cast<std::uint8_t>(hashmap_storage_kind::heap)) != 0u &&
            old_slots != nullptr) {
            free_raw(old_slots);
        }

        if (arena_alloc_fn_ != nullptr) {
            set_storage_kind(hashmap_storage_kind::owns | hashmap_storage_kind::arena);
        } else {
            set_storage_kind(hashmap_storage_kind::owns | hashmap_storage_kind::heap);
        }
        return status::ok;
    }

    hashmap_strategy        strategy_     = hashmap_strategy::chaining;
    std::size_t             bucket_count_ = 0u;
    std::size_t             size_         = 0u;
    std::size_t             key_offset_   = 0u;
    std::size_t             value_offset_ = 0u;
    std::size_t             slot_stride_  = 0u;
    hashmap_chain_node**    buckets_      = nullptr;
    std::byte*              slots_        = nullptr;
    runtime_element_policy  key_policy_{};
    runtime_element_policy  value_policy_{};
    runtime_hash_key_policy hash_policy_{};
    hashmap_policy          policy_       = hashmap_policy::none;
    void*          arena_ctx_      = nullptr;
    raw_alloc_fn   arena_alloc_fn_ = nullptr;
    hashmap_storage_kind    storage_kind_   = hashmap_storage_kind::external;
};

} // namespace memkit::detail
