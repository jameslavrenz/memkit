#pragma once

#include "../detail/compare_policy.hpp"
#include "../detail/element_policy.hpp"
#include "../detail/flat_map_core.hpp"
#include "../detail/utility.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace memkit {

enum class flat_map_policy : unsigned {
    none = 0u,
};

/** Sorted flat array map for tiny key sets (O(log n) lookup, O(n) insert). */
template<typename K, typename V, typename Compare = detail::type_compare<K>>
class FlatMap {
public:
    FlatMap() noexcept = default;

    FlatMap(FlatMap&& other) noexcept
        : core_{std::move(other.core_)}
        , compare_{std::move(other.compare_)}
    {
        other.core_.reset_state();
    }

    FlatMap& operator=(FlatMap&& other) noexcept
    {
        if (this != &other) {
            clear();
            core_    = std::move(other.core_);
            compare_ = std::move(other.compare_);
            other.core_.reset_state();
        }
        return *this;
    }

    FlatMap(const FlatMap&)            = delete;
    FlatMap& operator=(const FlatMap&) = delete;

    ~FlatMap() { clear(); }

    [[nodiscard]] static constexpr std::size_t entry_stride() noexcept
    {
        return detail::flat_map_core<
            detail::typed_element_policy<K>,
            detail::typed_element_policy<V>,
            detail::runtime_compare_policy>::entry_stride(sizeof(K), sizeof(V));
    }

    [[nodiscard]] static constexpr std::size_t storage_bytes(std::size_t capacity) noexcept
    {
        return capacity * entry_stride();
    }

    template<std::size_t Capacity>
    [[nodiscard]] static constexpr std::size_t storage_bytes() noexcept
    {
        return Capacity * entry_stride();
    }

    [[nodiscard]] status init(std::byte* entry_storage, std::size_t capacity) noexcept
    {
        if (entry_storage == nullptr || capacity == 0u) {
            return status::invalid;
        }

        clear();
        return core_.init(
            key_policy_,
            value_policy_,
            key_compare_policy{compare_},
            entry_storage,
            capacity
        );
    }

    template<std::size_t Capacity>
    [[nodiscard]] status init(stl::array<std::byte, storage_bytes<Capacity>()>& storage) noexcept
    {
        return init(storage.data(), Capacity);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t capacity)
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(storage_bytes(capacity), alignof(std::max_align_t), &ptr);
        if (!ok(st)) {
            return st;
        }

        return init(static_cast<std::byte*>(ptr), capacity);
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status get(const K& key, V& out) const
    {
        return core_.get(&key, &out);
    }

    [[nodiscard]] bool contains(const K& key) const { return core_.contains(&key); }

    [[nodiscard]] status put(const K& key, const V& value) { return core_.put(&key, &value); }
    [[nodiscard]] status put(const K& key, V&& value)
    {
        const V tmp(std::move(value));
        return core_.put(&key, &tmp);
    }

    [[nodiscard]] status remove(const K& key) { return core_.remove(&key); }
    [[nodiscard]] status remove(const K& key, V& out) { return core_.remove(&key, &out); }

private:
    struct key_compare_policy {
        Compare cmp{};

        [[nodiscard]] int compare(const void* a, const void* b) const noexcept
        {
            return cmp(*static_cast<const K*>(a), *static_cast<const K*>(b));
        }
    };

    using map_core = detail::flat_map_core<
        detail::typed_element_policy<K>,
        detail::typed_element_policy<V>,
        key_compare_policy>;

    map_core                               core_{};
    [[no_unique_address]] Compare          compare_{};
    detail::typed_element_policy<K>        key_policy_{};
    detail::typed_element_policy<V>        value_policy_{};
};

} // namespace memkit
