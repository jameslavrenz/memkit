#pragma once

#include "../detail/element_policy.hpp"
#include "../detail/hash_policy.hpp"
#include "../detail/lrucache_map_core.hpp"
#include "../status.hpp"

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace memkit {

using lrucache_policy = detail::lrucache_map_policy;

template<typename K, typename V, typename Hash = detail::type_hash<K>, typename Eq = detail::type_equal<K>>
class LruCache {
public:
    LruCache() noexcept = default;

    LruCache(LruCache&& other) noexcept
        : core_{std::move(other.core_)}
        , hash_{std::move(other.hash_)}
        , eq_{std::move(other.eq_)}
    {
        other.core_.reset_state();
        rebuild_policies();
        other.rebuild_policies();
    }

    LruCache& operator=(LruCache&& other) noexcept
    {
        if (this != &other) {
            clear();
            core_  = std::move(other.core_);
            hash_  = std::move(other.hash_);
            eq_    = std::move(other.eq_);
            other.core_.reset_state();
            rebuild_policies();
            other.rebuild_policies();
        }
        return *this;
    }

    LruCache(const LruCache&)            = delete;
    LruCache& operator=(const LruCache&) = delete;

    ~LruCache() { clear(); }

    [[nodiscard]] static std::size_t entry_stride() noexcept
    {
        return detail::lrucache_map_core<
            detail::runtime_element_policy,
            detail::runtime_element_policy>::entry_stride(sizeof(K), sizeof(V));
    }

    [[nodiscard]] static std::size_t entry_pool_bytes(std::size_t capacity) noexcept
    {
        return detail::lrucache_map_core<
            detail::runtime_element_policy,
            detail::runtime_element_policy>::entry_pool_bytes(capacity, sizeof(K), sizeof(V));
    }

    [[nodiscard]] static std::size_t buckets_bytes(std::size_t bucket_count) noexcept
    {
        return detail::lrucache_map_core<
            detail::runtime_element_policy,
            detail::runtime_element_policy>::buckets_bytes(bucket_count);
    }

    [[nodiscard]] static std::size_t default_bucket_count(std::size_t capacity) noexcept
    {
        return detail::lrucache_map_core<
            detail::runtime_element_policy,
            detail::runtime_element_policy>::default_bucket_count(capacity);
    }

    [[nodiscard]] status init(
        std::byte* entry_pool,
        std::size_t capacity,
        detail::lrucache_entry_header** buckets,
        std::size_t bucket_count
    ) noexcept
    {
        if (entry_pool == nullptr || buckets == nullptr || capacity == 0u || bucket_count == 0u) {
            return status::invalid;
        }

        clear();
        rebuild_policies();

        return core_.init(
            key_policy_,
            value_policy_,
            hash_policy_,
            entry_pool,
            capacity,
            buckets,
            bucket_count
        );
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t capacity, std::size_t bucket_count = 0u)
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        clear();
        rebuild_policies();

        if (bucket_count == 0u) {
            bucket_count = default_bucket_count(capacity);
        }

        void* pool_ptr = nullptr;
        status st = arena.allocate(entry_pool_bytes(capacity), alignof(std::max_align_t), &pool_ptr);
        if (!ok(st)) {
            return st;
        }

        void* bucket_ptr = nullptr;
        st = arena.allocate(buckets_bytes(bucket_count), alignof(detail::lrucache_entry_header*), &bucket_ptr);
        if (!ok(st)) {
            return st;
        }

        core_.set_storage_kind(
            detail::lrucache_storage_kind::owns | detail::lrucache_storage_kind::arena
        );
        core_.set_owns_buckets(true);

        return core_.init(
            key_policy_,
            value_policy_,
            hash_policy_,
            static_cast<std::byte*>(pool_ptr),
            capacity,
            static_cast<detail::lrucache_entry_header**>(bucket_ptr),
            bucket_count
        );
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] std::size_t bucket_count() const noexcept { return core_.bucket_count(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status get(const K& key, V& out) { return core_.get(&key, &out); }

    [[nodiscard]] status peek(const K& key, V& out) const { return core_.peek(&key, &out); }

    [[nodiscard]] status put(const K& key, const V& value) { return put_impl(key, value); }
    [[nodiscard]] status put(K&& key, const V& value) { return put_impl(std::move(key), value); }
    [[nodiscard]] status put(const K& key, V&& value) { return put_impl(key, std::move(value)); }
    [[nodiscard]] status put(K&& key, V&& value) { return put_impl(std::move(key), std::move(value)); }

    [[nodiscard]] status remove(const K& key) { return core_.remove(&key); }

    [[nodiscard]] bool contains(const K& key) const { return core_.contains(&key); }

    [[nodiscard]] status touch(const K& key) { return core_.touch(&key); }

    template<typename Visitor>
    [[nodiscard]] status foreach_mru(Visitor&& visit) const
    {
        return core_.foreach_mru([&visit](const void* key_ptr, const void* value_ptr) {
            return visit(*static_cast<const K*>(key_ptr), *static_cast<const V*>(value_ptr));
        });
    }

    template<typename Visitor>
    [[nodiscard]] status foreach_lru(Visitor&& visit) const
    {
        return core_.foreach_lru([&visit](const void* key_ptr, const void* value_ptr) {
            return visit(*static_cast<const K*>(key_ptr), *static_cast<const V*>(value_ptr));
        });
    }

private:
    using map_core = detail::lrucache_map_core<
        detail::runtime_element_policy,
        detail::runtime_element_policy>;

    template<typename KeyArg, typename ValueArg>
    [[nodiscard]] status put_impl(KeyArg&& key, ValueArg&& value)
    {
        K key_copy(std::forward<KeyArg>(key));
        V value_copy(std::forward<ValueArg>(value));
        return core_.put(&key_copy, &value_copy);
    }

    void rebuild_policies() noexcept
    {
        key_policy_ = detail::runtime_element_policy{
            sizeof(K),
            &LruCache::key_copy_trampoline,
            &LruCache::key_destroy_trampoline,
            this,
        };
        value_policy_ = detail::runtime_element_policy{
            sizeof(V),
            &LruCache::value_copy_trampoline,
            &LruCache::value_destroy_trampoline,
            this,
        };
        hash_policy_ = detail::runtime_hash_key_policy{
            sizeof(K),
            &LruCache::hash_trampoline,
            &LruCache::key_eq_trampoline,
            this,
        };
    }

    static status key_copy_trampoline(void* dst, const void* src, void* user)
    {
        static_cast<LruCache*>(user)->key_typed_.copy_construct(dst, src);
        return status::ok;
    }

    static status value_copy_trampoline(void* dst, const void* src, void* user)
    {
        static_cast<LruCache*>(user)->value_typed_.copy_construct(dst, src);
        return status::ok;
    }

    static void key_destroy_trampoline(void* elem, void* user)
    {
        static_cast<LruCache*>(user)->key_typed_.destroy(elem);
    }

    static void value_destroy_trampoline(void* elem, void* user)
    {
        static_cast<LruCache*>(user)->value_typed_.destroy(elem);
    }

    static std::size_t hash_trampoline(const void* key, std::size_t key_size, void* user)
    {
        auto* self = static_cast<LruCache*>(user);
        (void)key_size;
        return self->hash_(*static_cast<const K*>(key));
    }

    static bool key_eq_trampoline(
        const void* a,
        const void* b,
        std::size_t key_size,
        void* user
    )
    {
        auto* self = static_cast<LruCache*>(user);
        (void)key_size;
        return self->eq_(*static_cast<const K*>(a), *static_cast<const K*>(b));
    }

    map_core                            core_{};
    [[no_unique_address]] Hash          hash_{};
    [[no_unique_address]] Eq            eq_{};
    detail::typed_element_policy<K>     key_typed_{};
    detail::typed_element_policy<V>     value_typed_{};
    detail::runtime_element_policy      key_policy_{};
    detail::runtime_element_policy      value_policy_{};
    detail::runtime_hash_key_policy     hash_policy_{};
};

} // namespace memkit
