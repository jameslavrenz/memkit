#pragma once

#include "../config.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace memkit::detail {

[[nodiscard]] inline std::size_t fnv1a_bytes(const void* data, std::size_t size) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::size_t hash  = 2166136261u;

    for (std::size_t i = 0u; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

template<typename T>
struct type_hash {
    [[nodiscard]] std::size_t operator()(const T& key) const noexcept
    {
        if constexpr (requires { stl::hash<T>{}(key); }) {
            return stl::hash<T>{}(key);
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            return fnv1a_bytes(&key, sizeof(T));
        } else {
            static_assert(sizeof(T) == 0, "Provide a custom Hash functor for non-trivial types");
            return 0u;
        }
    }
};

template<typename T>
struct type_equal {
    [[nodiscard]] bool operator()(const T& a, const T& b) const noexcept
    {
        if constexpr (requires { stl::equal_to<T>{}(a, b); }) {
            return stl::equal_to<T>{}(a, b);
        } else if constexpr (requires { { a == b } -> std::convertible_to<bool>; }) {
            return a == b;
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            return std::memcmp(&a, &b, sizeof(T)) == 0;
        } else {
            static_assert(sizeof(T) == 0, "Provide a custom Eq functor for non-trivial types");
            return false;
        }
    }
};

struct runtime_hash_key_policy {
    using hash_fn   = std::size_t (*)(const void* key, std::size_t key_size, void* user);
    using key_eq_fn = bool (*)(const void* a, const void* b, std::size_t key_size, void* user);

    std::size_t key_size_  = 0u;
    hash_fn     hash_fn_   = nullptr;
    key_eq_fn   key_eq_fn_ = nullptr;
    void*       user_      = nullptr;

    runtime_hash_key_policy() = default;

    runtime_hash_key_policy(
        std::size_t key_size,
        hash_fn hash,
        key_eq_fn key_eq,
        void* user
    ) noexcept
        : key_size_(key_size)
        , hash_fn_(hash)
        , key_eq_fn_(key_eq)
        , user_(user)
    {}

    [[nodiscard]] std::size_t hash(const void* key) const noexcept
    {
        if (hash_fn_ != nullptr) {
            return hash_fn_(key, key_size_, user_);
        }
        return fnv1a_bytes(key, key_size_);
    }

    [[nodiscard]] bool equal(const void* a, const void* b) const noexcept
    {
        if (key_eq_fn_ != nullptr) {
            return key_eq_fn_(a, b, key_size_, user_);
        }
        return std::memcmp(a, b, key_size_) == 0;
    }
};

} // namespace memkit::detail
