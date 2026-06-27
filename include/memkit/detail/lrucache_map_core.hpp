#pragma once

#include "../status.hpp"
#include "element_policy.hpp"
#include "hash_policy.hpp"
#include "utility.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

namespace memkit::detail {

enum class lrucache_map_policy : std::uint8_t {
    none         = 0u,
    fixed_pool   = 1u << 0u,
};

enum class lrucache_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline lrucache_storage_kind operator|(
    lrucache_storage_kind a,
    lrucache_storage_kind b
) noexcept
{
    return static_cast<lrucache_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

struct lrucache_entry_header {
    lrucache_entry_header* hash_next = nullptr;
    lrucache_entry_header* lru_prev  = nullptr;
    lrucache_entry_header* lru_next  = nullptr;
};

template<
    typename KeyPolicy,
    typename ValuePolicy,
    typename HashEqPolicy = runtime_hash_key_policy
>
class lrucache_map_core {
public:
    lrucache_map_core() = default;

    [[nodiscard]] static std::size_t entry_stride(
        std::size_t key_size,
        std::size_t value_size
    ) noexcept
    {
        const std::size_t header =
            align_up(sizeof(lrucache_entry_header), alignof(lrucache_entry_header));
        return header + key_size + value_size;
    }

    [[nodiscard]] static std::size_t entry_pool_bytes(
        std::size_t capacity,
        std::size_t key_size,
        std::size_t value_size
    ) noexcept
    {
        return capacity * entry_stride(key_size, value_size);
    }

    [[nodiscard]] static std::size_t buckets_bytes(std::size_t bucket_count) noexcept
    {
        return bucket_count * sizeof(lrucache_entry_header*);
    }

    [[nodiscard]] static std::size_t default_bucket_count(std::size_t capacity) noexcept
    {
        constexpr std::size_t min_buckets = 4u;
        std::size_t buckets               = min_buckets;

        while (buckets < capacity) {
            if (buckets > SIZE_MAX / 2u) {
                return capacity;
            }
            buckets *= 2u;
        }

        return buckets;
    }

    [[nodiscard]] status init(
        KeyPolicy key_policy,
        ValuePolicy value_policy,
        HashEqPolicy hash_eq_policy,
        std::byte* entry_pool,
        std::size_t capacity,
        lrucache_entry_header** buckets,
        std::size_t bucket_count,
        lrucache_map_policy map_flags = lrucache_map_policy::fixed_pool
    ) noexcept
    {
        if (entry_pool == nullptr || buckets == nullptr || capacity == 0u || bucket_count == 0u) {
            return status::invalid;
        }

        key_policy_     = key_policy;
        value_policy_   = value_policy;
        hash_eq_policy_ = std::move(hash_eq_policy);
        entry_pool_    = entry_pool;
        buckets_       = buckets;
        capacity_      = capacity;
        bucket_count_  = bucket_count;
        map_flags_     = map_flags;
        storage_kind_  = lrucache_storage_kind::external;
        owns_buckets_  = false;
        return reset_free_list();
    }

    void reset_state() noexcept
    {
        key_policy_     = KeyPolicy{};
        value_policy_   = ValuePolicy{};
        hash_eq_policy_ = HashEqPolicy{};
        buckets_       = nullptr;
        lru_head_      = nullptr;
        lru_tail_      = nullptr;
        free_list_     = nullptr;
        entry_pool_    = nullptr;
        capacity_      = 0u;
        bucket_count_  = 0u;
        size_          = 0u;
        map_flags_     = lrucache_map_policy::none;
        storage_kind_  = lrucache_storage_kind::external;
        owns_buckets_  = false;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t bucket_count() const noexcept { return bucket_count_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }
    [[nodiscard]] bool full() const noexcept { return size_ >= capacity_; }
    [[nodiscard]] lrucache_map_policy flags() const noexcept { return map_flags_; }
    [[nodiscard]] lrucache_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] std::byte* entry_pool() const noexcept { return entry_pool_; }
    [[nodiscard]] lrucache_entry_header** buckets() const noexcept { return buckets_; }
    [[nodiscard]] std::size_t entry_stride() const noexcept
    {
        return entry_stride(key_policy_.elem_size(), value_policy_.elem_size());
    }

    void set_storage_kind(lrucache_storage_kind kind) noexcept { storage_kind_ = kind; }
    void set_owns_buckets(bool owns) noexcept { owns_buckets_ = owns; }
    [[nodiscard]] bool owns_buckets() const noexcept { return owns_buckets_; }

    void clear() noexcept
    {
        if (size_ == 0u) {
            return;
        }

        for (std::size_t i = 0u; i < bucket_count_; ++i) {
            lrucache_entry_header* current = buckets_[i];
            while (current != nullptr) {
                lrucache_entry_header* const next = current->hash_next;
                destroy_entry(current);
                release_entry(current);
                current = next;
            }
            buckets_[i] = nullptr;
        }

        lru_head_ = nullptr;
        lru_tail_ = nullptr;
        size_     = 0u;
    }

    [[nodiscard]] status get(const void* key, void* out_value) noexcept
    {
        if (key == nullptr) {
            return status::null_ptr;
        }

        lrucache_entry_header* const found = find_entry(key, nullptr, nullptr);
        if (found == nullptr) {
            return status::not_found;
        }

        if (out_value != nullptr) {
            if constexpr (std::is_same_v<ValuePolicy, runtime_element_policy>) {
                const status copy_st = value_policy_.copy_construct(
                    out_value,
                    entry_value_const(found)
                );
                if (!ok(copy_st)) {
                    return copy_st;
                }
            } else {
                value_policy_.copy_construct(out_value, entry_value_const(found));
            }
        }

        promote(found);
        return status::ok;
    }

    [[nodiscard]] status peek(const void* key, void* out_value) const noexcept
    {
        if (key == nullptr) {
            return status::null_ptr;
        }

        const lrucache_entry_header* const found = find_entry(key, nullptr, nullptr);
        if (found == nullptr) {
            return status::not_found;
        }

        if (out_value == nullptr) {
            return status::ok;
        }

        if constexpr (std::is_same_v<ValuePolicy, runtime_element_policy>) {
            return value_policy_.copy_construct(out_value, entry_value_const(found));
        } else {
            value_policy_.copy_construct(out_value, entry_value_const(found));
            return status::ok;
        }
    }

    [[nodiscard]] status put(const void* key, const void* value) noexcept
    {
        if (key == nullptr || value == nullptr) {
            return status::null_ptr;
        }

        lrucache_entry_header* const existing = find_entry(key, nullptr, nullptr);
        if (existing != nullptr) {
            if constexpr (std::is_same_v<ValuePolicy, runtime_element_policy>) {
                const status copy_st = value_policy_.copy_construct(
                    entry_value(existing),
                    value
                );
                if (!ok(copy_st)) {
                    return copy_st;
                }
            } else {
                value_policy_.copy_assign(entry_value(existing), value);
            }
            promote(existing);
            return status::ok;
        }

        lrucache_entry_header* slot = nullptr;
        std::size_t bucket_index    = 0u;
        const status prepare_st     = prepare_entry(key, value, &slot, &bucket_index);
        if (!ok(prepare_st)) {
            return prepare_st;
        }

        slot->hash_next        = buckets_[bucket_index];
        buckets_[bucket_index] = slot;
        lru_push_front(slot);
        ++size_;
        return status::ok;
    }

    [[nodiscard]] status remove(const void* key) noexcept
    {
        if (key == nullptr) {
            return status::null_ptr;
        }

        std::size_t bucket_index = 0u;
        lrucache_entry_header* prev = nullptr;
        lrucache_entry_header* const found = find_entry(key, &bucket_index, &prev);
        if (found == nullptr) {
            return status::not_found;
        }

        destroy_entry(found);
        lru_unlink(found);
        hash_unlink(bucket_index, found, prev);
        release_entry(found);
        --size_;
        return status::ok;
    }

    [[nodiscard]] bool contains(const void* key) const noexcept
    {
        if (key == nullptr) {
            return false;
        }
        return find_entry(key, nullptr, nullptr) != nullptr;
    }

    [[nodiscard]] status touch(const void* key) noexcept
    {
        if (key == nullptr) {
            return status::null_ptr;
        }

        lrucache_entry_header* const found = find_entry(key, nullptr, nullptr);
        if (found == nullptr) {
            return status::not_found;
        }

        promote(found);
        return status::ok;
    }

    template<typename Visitor>
    [[nodiscard]] status foreach_mru(Visitor&& visit) const
    {
        return foreach_list(std::forward<Visitor>(visit), true);
    }

    template<typename Visitor>
    [[nodiscard]] status foreach_lru(Visitor&& visit) const
    {
        return foreach_list(std::forward<Visitor>(visit), false);
    }

private:
    [[nodiscard]] lrucache_entry_header* entry_at(std::size_t index) const noexcept
    {
        return reinterpret_cast<lrucache_entry_header*>(entry_pool_ + index * entry_stride());
    }

    [[nodiscard]] status reset_free_list() noexcept
    {
        free_list_ = nullptr;
        lru_head_  = nullptr;
        lru_tail_  = nullptr;
        size_      = 0u;

        std::memset(buckets_, 0, buckets_bytes(bucket_count_));

        for (std::size_t i = 0u; i < capacity_; ++i) {
            lrucache_entry_header* const slot = entry_at(i);
            slot->lru_prev                    = nullptr;
            slot->lru_next                    = nullptr;
            slot->hash_next                   = free_list_;
            free_list_                        = slot;
        }

        return status::ok;
    }

    [[nodiscard]] const lrucache_entry_header* find_entry(
        const void* key,
        std::size_t* out_bucket,
        lrucache_entry_header** out_prev
    ) const noexcept
    {
        const std::size_t bucket_index = hash_eq_policy_.hash(key) % bucket_count_;
        lrucache_entry_header* prev    = nullptr;
        lrucache_entry_header* current = buckets_[bucket_index];

        while (current != nullptr) {
            if (hash_eq_policy_.equal(entry_key_const(current), key)) {
                if (out_bucket != nullptr) {
                    *out_bucket = bucket_index;
                }
                if (out_prev != nullptr) {
                    *out_prev = prev;
                }
                return current;
            }
            prev    = current;
            current = current->hash_next;
        }

        if (out_bucket != nullptr) {
            *out_bucket = bucket_index;
        }
        if (out_prev != nullptr) {
            *out_prev = prev;
        }
        return nullptr;
    }

    [[nodiscard]] lrucache_entry_header* find_entry(
        const void* key,
        std::size_t* out_bucket,
        lrucache_entry_header** out_prev
    ) noexcept
    {
        return const_cast<lrucache_entry_header*>(
            static_cast<const lrucache_map_core*>(this)->find_entry(key, out_bucket, out_prev)
        );
    }

    void lru_unlink(lrucache_entry_header* slot) noexcept
    {
        if (slot->lru_prev != nullptr) {
            slot->lru_prev->lru_next = slot->lru_next;
        } else {
            lru_head_ = slot->lru_next;
        }

        if (slot->lru_next != nullptr) {
            slot->lru_next->lru_prev = slot->lru_prev;
        } else {
            lru_tail_ = slot->lru_prev;
        }

        slot->lru_prev = nullptr;
        slot->lru_next = nullptr;
    }

    void lru_push_front(lrucache_entry_header* slot) noexcept
    {
        slot->lru_prev = nullptr;
        slot->lru_next = lru_head_;

        if (lru_head_ != nullptr) {
            lru_head_->lru_prev = slot;
        } else {
            lru_tail_ = slot;
        }

        lru_head_ = slot;
    }

    void promote(lrucache_entry_header* slot) noexcept
    {
        if (lru_head_ == slot) {
            return;
        }

        lru_unlink(slot);
        lru_push_front(slot);
    }

    void hash_unlink(
        std::size_t bucket_index,
        lrucache_entry_header* slot,
        lrucache_entry_header* prev
    ) noexcept
    {
        if (prev == nullptr) {
            buckets_[bucket_index] = slot->hash_next;
        } else {
            prev->hash_next = slot->hash_next;
        }
        slot->hash_next = nullptr;
    }

    [[nodiscard]] lrucache_entry_header* acquire_entry() noexcept
    {
        if (free_list_ == nullptr) {
            return nullptr;
        }

        lrucache_entry_header* const slot = free_list_;
        free_list_                        = slot->hash_next;
        slot->hash_next                   = nullptr;
        return slot;
    }

    void release_entry(lrucache_entry_header* slot) noexcept
    {
        slot->lru_prev  = nullptr;
        slot->lru_next  = nullptr;
        slot->hash_next = free_list_;
        free_list_      = slot;
    }

    void destroy_entry(lrucache_entry_header* slot) noexcept
    {
        if (key_policy_.needs_destroy_on_clear()) {
            key_policy_.destroy(entry_key(slot));
        }
        if (value_policy_.needs_destroy_on_clear()) {
            value_policy_.destroy(entry_value(slot));
        }
    }

    [[nodiscard]] status prepare_entry(
        const void* key,
        const void* value,
        lrucache_entry_header** out_entry,
        std::size_t* out_bucket
    ) noexcept
    {
        lrucache_entry_header* slot = acquire_entry();
        const bool from_free        = slot != nullptr;

        if (!from_free) {
            if (size_ < capacity_ || lru_tail_ == nullptr) {
                return status::full;
            }

            slot = lru_tail_;
            std::size_t old_bucket = 0u;
            lrucache_entry_header* prev = nullptr;
            (void)find_entry(entry_key_const(slot), &old_bucket, &prev);

            destroy_entry(slot);
            lru_unlink(slot);
            hash_unlink(old_bucket, slot, prev);
            --size_;
        }

        if constexpr (std::is_same_v<KeyPolicy, runtime_element_policy>) {
            const status key_st = key_policy_.copy_construct(entry_key(slot), key);
            if (!ok(key_st)) {
                if (from_free) {
                    release_entry(slot);
                }
                return key_st;
            }
        } else {
            key_policy_.copy_construct(entry_key(slot), key);
        }

        if constexpr (std::is_same_v<ValuePolicy, runtime_element_policy>) {
            const status value_st = value_policy_.copy_construct(entry_value(slot), value);
            if (!ok(value_st)) {
                key_policy_.destroy(entry_key(slot));
                if (from_free) {
                    release_entry(slot);
                }
                return value_st;
            }
        } else {
            value_policy_.copy_construct(entry_value(slot), value);
        }

        *out_bucket = hash_eq_policy_.hash(key) % bucket_count_;
        *out_entry  = slot;
        return status::ok;
    }

    template<typename Visitor>
    [[nodiscard]] status foreach_list(Visitor&& visit, bool mru_order) const
    {
        lrucache_entry_header* current = mru_order ? lru_head_ : lru_tail_;
        while (current != nullptr) {
            const status st = visit(entry_key_const(current), entry_value_const(current));
            if (!ok(st)) {
                return st;
            }
            current = mru_order ? current->lru_next : current->lru_prev;
        }
        return status::ok;
    }

    [[nodiscard]] void* entry_key(lrucache_entry_header* entry) const noexcept
    {
        const std::size_t header =
            align_up(sizeof(lrucache_entry_header), alignof(lrucache_entry_header));
        return reinterpret_cast<std::byte*>(entry) + header;
    }

    [[nodiscard]] void* entry_value(lrucache_entry_header* entry) const noexcept
    {
        return static_cast<std::byte*>(entry_key(entry)) + key_policy_.elem_size();
    }

    [[nodiscard]] const void* entry_key_const(const lrucache_entry_header* entry) const noexcept
    {
        return entry_key(const_cast<lrucache_entry_header*>(entry));
    }

    [[nodiscard]] const void* entry_value_const(const lrucache_entry_header* entry) const noexcept
    {
        return entry_value(const_cast<lrucache_entry_header*>(entry));
    }

    KeyPolicy                key_policy_{};
    ValuePolicy              value_policy_{};
    HashEqPolicy             hash_eq_policy_{};
    lrucache_entry_header**  buckets_      = nullptr;
    lrucache_entry_header*   lru_head_     = nullptr;
    lrucache_entry_header*   lru_tail_     = nullptr;
    lrucache_entry_header*   free_list_    = nullptr;
    std::byte*               entry_pool_   = nullptr;
    std::size_t              capacity_     = 0u;
    std::size_t              bucket_count_ = 0u;
    std::size_t              size_         = 0u;
    lrucache_map_policy      map_flags_    = lrucache_map_policy::none;
    lrucache_storage_kind    storage_kind_ = lrucache_storage_kind::external;
    bool                     owns_buckets_ = false;
};

} // namespace memkit::detail
