#pragma once

#include "../status.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace memkit {

/** Dense array map keyed by enum (O(1), no hash/compare). */
template<typename Enum, typename V, std::size_t N>
class EnumMap {
public:
    static_assert(std::is_enum_v<Enum>, "EnumMap key type must be an enum");

    EnumMap() noexcept = default;

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return N; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }

    void clear() noexcept
    {
        for (std::size_t i = 0u; i < N; ++i) {
            present_[i] = false;
        }
        size_ = 0u;
    }

    [[nodiscard]] status put(Enum key, const V& value) noexcept
    {
        const std::size_t index = key_index(key);
        if (index >= N) {
            return status::invalid;
        }

        if (!present_[index]) {
            ++size_;
            present_[index] = true;
        }

        values_[index] = value;
        return status::ok;
    }

    [[nodiscard]] status put(Enum key, V&& value) noexcept
    {
        const std::size_t index = key_index(key);
        if (index >= N) {
            return status::invalid;
        }

        if (!present_[index]) {
            ++size_;
            present_[index] = true;
        }

        values_[index] = std::move(value);
        return status::ok;
    }

    [[nodiscard]] status get(Enum key, V& out) const noexcept
    {
        const std::size_t index = key_index(key);
        if (index >= N || !present_[index]) {
            return status::not_found;
        }

        out = values_[index];
        return status::ok;
    }

    [[nodiscard]] bool contains(Enum key) const noexcept
    {
        const std::size_t index = key_index(key);
        return index < N && present_[index];
    }

    [[nodiscard]] status remove(Enum key) noexcept
    {
        const std::size_t index = key_index(key);
        if (index >= N || !present_[index]) {
            return status::not_found;
        }

        present_[index] = false;
        values_[index]  = V{};
        --size_;
        return status::ok;
    }

    [[nodiscard]] V* try_get(Enum key) noexcept
    {
        const std::size_t index = key_index(key);
        if (index >= N || !present_[index]) {
            return nullptr;
        }
        return &values_[index];
    }

    [[nodiscard]] const V* try_get(Enum key) const noexcept
    {
        const std::size_t index = key_index(key);
        if (index >= N || !present_[index]) {
            return nullptr;
        }
        return &values_[index];
    }

private:
    [[nodiscard]] static std::size_t key_index(Enum key) noexcept
    {
        return static_cast<std::size_t>(key);
    }

    V     values_[N]{};
    bool  present_[N]{};
    std::size_t size_ = 0u;
};

} // namespace memkit
