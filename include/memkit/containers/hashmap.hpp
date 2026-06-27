#pragma once

#include "../detail/element_policy.hpp"
#include "../detail/hash_policy.hpp"
#include "../detail/hashmap_map_core.hpp"
#include "../status.hpp"

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace memkit {

using hashmap_strategy = detail::hashmap_strategy;
using hashmap_policy   = detail::hashmap_policy;

template<typename K, typename V, typename Hash = detail::type_hash<K>, typename Eq = detail::type_equal<K>>
class HashMap {
public:
    struct open_slot {
        enum class state : std::uint8_t {
            empty    = 0,
            deleted  = 1,
            occupied = 2,
        };

        state slot_state = state::empty;
        K     key{};
        V     value{};
    };

    HashMap() noexcept = default;

    HashMap(HashMap&& other) noexcept
        : core_{std::move(other.core_)}
        , hash_{std::move(other.hash_)}
        , eq_{std::move(other.eq_)}
    {
        other.core_.reset_state();
        rebuild_policies();
        other.rebuild_policies();
    }

    HashMap& operator=(HashMap&& other) noexcept
    {
        if (this != &other) {
            clear();
            release_owned_storage();
            core_ = std::move(other.core_);
            hash_ = std::move(other.hash_);
            eq_   = std::move(other.eq_);
            other.core_.reset_state();
            rebuild_policies();
            other.rebuild_policies();
        }
        return *this;
    }

    HashMap(const HashMap&)            = delete;
    HashMap& operator=(const HashMap&) = delete;

    ~HashMap() { clear(); release_owned_storage(); }

    [[nodiscard]] status init(
        open_slot* slots,
        std::size_t slot_count,
        hashmap_policy policy = hashmap_policy::none
    ) noexcept
    {
        if (slots == nullptr || slot_count == 0u) {
            return status::invalid;
        }

        clear();
        release_owned_storage();
        rebuild_policies();

        return core_.init_open(
            reinterpret_cast<std::byte*>(slots),
            slot_count,
            offsetof(open_slot, key),
            offsetof(open_slot, value),
            sizeof(open_slot),
            key_policy_,
            value_policy_,
            hash_policy_,
            policy
        );
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t initial_buckets,
        hashmap_strategy strategy = hashmap_strategy::chaining,
        hashmap_policy policy     = hashmap_policy::growable
    )
    {
        clear();
        release_owned_storage();
        rebuild_policies();

        return core_.init_from_arena(
            arena,
            initial_buckets,
            strategy,
            key_policy_,
            value_policy_,
            hash_policy_,
            policy
        );
    }

    [[nodiscard]] hashmap_strategy strategy() const noexcept { return core_.strategy(); }
    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t bucket_count() const noexcept { return core_.bucket_count(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status put(const K& key, const V& value) { return put_impl(key, value); }
    [[nodiscard]] status put(K&& key, const V& value) { return put_impl(std::move(key), value); }
    [[nodiscard]] status put(const K& key, V&& value) { return put_impl(key, std::move(value)); }
    [[nodiscard]] status put(K&& key, V&& value) { return put_impl(std::move(key), std::move(value)); }

    [[nodiscard]] status get(const K& key, V& out) const
    {
        return core_.get(&key, &out);
    }

    [[nodiscard]] status remove(const K& key) { return core_.remove(&key); }

    [[nodiscard]] bool contains(const K& key) const { return core_.contains(&key); }

    template<typename Visitor>
    [[nodiscard]] status foreach(Visitor&& visit) const
    {
        return core_.foreach([&visit](const void* key_ptr, const void* value_ptr) {
            return visit(
                *static_cast<const K*>(key_ptr),
                *static_cast<const V*>(value_ptr)
            );
        });
    }

private:
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
            &HashMap::key_copy_trampoline,
            &HashMap::key_destroy_trampoline,
            this,
        };
        value_policy_ = detail::runtime_element_policy{
            sizeof(V),
            &HashMap::value_copy_trampoline,
            &HashMap::value_destroy_trampoline,
            this,
        };
        hash_policy_ = detail::runtime_hash_key_policy{
            sizeof(K),
            &HashMap::hash_trampoline,
            &HashMap::key_eq_trampoline,
            this,
        };
    }

    static status key_copy_trampoline(void* dst, const void* src, void* user)
    {
        auto* self = static_cast<HashMap*>(user);
        self->key_typed_.copy_construct(dst, src);
        return status::ok;
    }

    static status value_copy_trampoline(void* dst, const void* src, void* user)
    {
        auto* self = static_cast<HashMap*>(user);
        self->value_typed_.copy_construct(dst, src);
        return status::ok;
    }

    static void key_destroy_trampoline(void* elem, void* user)
    {
        static_cast<HashMap*>(user)->key_typed_.destroy(elem);
    }

    static void value_destroy_trampoline(void* elem, void* user)
    {
        static_cast<HashMap*>(user)->value_typed_.destroy(elem);
    }

    static std::size_t hash_trampoline(const void* key, std::size_t key_size, void* user)
    {
        auto* self = static_cast<HashMap*>(user);
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
        auto* self = static_cast<HashMap*>(user);
        (void)key_size;
        return self->eq_(*static_cast<const K*>(a), *static_cast<const K*>(b));
    }

    void release_owned_storage() noexcept
    {
        if ((static_cast<std::uint8_t>(core_.storage_kind()) &
             static_cast<std::uint8_t>(detail::hashmap_storage_kind::owns)) != 0u) {
            core_.release_storage();
        }
    }

    detail::hashmap_map_core       core_{};
    [[no_unique_address]] Hash     hash_{};
    [[no_unique_address]] Eq       eq_{};
    detail::typed_element_policy<K> key_typed_{};
    detail::typed_element_policy<V> value_typed_{};
    detail::runtime_element_policy  key_policy_{};
    detail::runtime_element_policy  value_policy_{};
    detail::runtime_hash_key_policy hash_policy_{};
};

} // namespace memkit
