#pragma once

#include "../status.hpp"
#include "compare_policy.hpp"
#include "element_policy.hpp"
#include "utility.hpp"

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace memkit::detail {

enum class flat_map_policy : std::uint8_t {
    none     = 0u,
    growable = 1u << 0u,
};

enum class flat_map_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

template<typename KeyPolicy, typename ValuePolicy, typename ComparePolicy>
class flat_map_core {
public:
    flat_map_core() = default;

    [[nodiscard]] static constexpr std::size_t entry_stride(
        std::size_t key_size,
        std::size_t value_size
    ) noexcept
    {
        const std::size_t value_off = align_up(key_size, bytes_alignment(value_size));
        return value_off + value_size;
    }

    [[nodiscard]] status init(
        KeyPolicy key_policy,
        ValuePolicy value_policy,
        ComparePolicy compare,
        std::byte* entry_storage,
        std::size_t capacity,
        flat_map_policy policy = flat_map_policy::none
    ) noexcept
    {
        if (entry_storage == nullptr || capacity == 0u) {
            return status::invalid;
        }

        key_policy_    = key_policy;
        value_policy_  = value_policy;
        compare_       = compare;
        storage_       = entry_storage;
        capacity_      = capacity;
        size_          = 0u;
        entry_stride_  = entry_stride(
            key_policy_.elem_size(),
            value_policy_.elem_size()
        );
        value_offset_  = align_up(key_policy_.elem_size(), value_policy_.alignment());
        map_policy_    = policy;
        storage_kind_  = flat_map_storage_kind::external;
        return status::ok;
    }

    void reset_state() noexcept
    {
        key_policy_   = KeyPolicy{};
        value_policy_ = ValuePolicy{};
        compare_      = ComparePolicy{};
        storage_      = nullptr;
        capacity_     = 0u;
        size_         = 0u;
        entry_stride_ = 0u;
        value_offset_ = 0u;
        storage_kind_ = flat_map_storage_kind::external;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }
    [[nodiscard]] bool full() const noexcept { return size_ >= capacity_; }

    void clear() noexcept
    {
        for (std::size_t i = 0u; i < size_; ++i) {
            destroy_entry_at(i);
        }
        size_ = 0u;
    }

    [[nodiscard]] status find_index(const void* key, std::size_t* out_index) const noexcept
    {
        if (key == nullptr || out_index == nullptr) {
            return status::null_ptr;
        }

        std::size_t lo = 0u;
        std::size_t hi = size_;
        while (lo < hi) {
            const std::size_t mid = lo + ((hi - lo) >> 1u);
            const int cmp         = compare_.compare(key_ptr_at(mid), key);
            if (cmp < 0) {
                lo = mid + 1u;
            } else if (cmp > 0) {
                hi = mid;
            } else {
                *out_index = mid;
                return status::ok;
            }
        }

        *out_index = lo;
        return status::not_found;
    }

    [[nodiscard]] status get(const void* key, void* out_value) const noexcept
    {
        std::size_t index = 0u;
        if (find_index(key, &index) != status::ok) {
            return status::not_found;
        }
        value_policy_.copy_construct(out_value, value_ptr_at(index));
        return status::ok;
    }

    [[nodiscard]] status put(const void* key, const void* value) noexcept
    {
        if (key == nullptr || value == nullptr) {
            return status::null_ptr;
        }

        std::size_t index = 0u;
        const status find_st = find_index(key, &index);
        if (find_st == status::ok) {
            value_policy_.destroy(value_ptr_at(index));
            value_policy_.copy_construct(value_ptr_at(index), value);
            return status::ok;
        }

        if (full()) {
            return status::full;
        }

        if (index < size_) {
            shift_right_from(index);
        }

        key_policy_.copy_construct(key_ptr_at(index), key);
        value_policy_.copy_construct(value_ptr_at(index), value);
        ++size_;
        return status::ok;
    }

    [[nodiscard]] status remove(const void* key, void* out_value = nullptr) noexcept
    {
        std::size_t index = 0u;
        if (find_index(key, &index) != status::ok) {
            return status::not_found;
        }

        if (out_value != nullptr) {
            value_policy_.copy_construct(out_value, value_ptr_at(index));
        }

        destroy_entry_at(index);
        shift_left_from(index + 1u);
        --size_;
        return status::ok;
    }

    [[nodiscard]] bool contains(const void* key) const noexcept
    {
        std::size_t index = 0u;
        return find_index(key, &index) == status::ok;
    }

    [[nodiscard]] flat_map_storage_kind storage_kind() const noexcept { return storage_kind_; }
    void set_storage_kind(flat_map_storage_kind kind) noexcept { storage_kind_ = kind; }

private:
    [[nodiscard]] std::byte* entry_at(std::size_t index) const noexcept
    {
        return storage_ + (index * entry_stride_);
    }

    [[nodiscard]] void* key_ptr_at(std::size_t index) const noexcept
    {
        return static_cast<void*>(entry_at(index));
    }

    [[nodiscard]] void* value_ptr_at(std::size_t index) const noexcept
    {
        return static_cast<void*>(entry_at(index) + value_offset_);
    }

    void destroy_entry_at(std::size_t index) noexcept
    {
        key_policy_.destroy(key_ptr_at(index));
        value_policy_.destroy(value_ptr_at(index));
    }

    void shift_right_from(std::size_t index) noexcept
    {
        for (std::size_t i = size_; i > index; --i) {
            move_entry(i - 1u, i);
        }
    }

    void shift_left_from(std::size_t index) noexcept
    {
        for (std::size_t i = index; i < size_; ++i) {
            move_entry(i, i - 1u);
        }
    }

    void move_entry(std::size_t from, std::size_t to) noexcept
    {
        key_policy_.move_construct(key_ptr_at(to), key_ptr_at(from));
        value_policy_.move_construct(value_ptr_at(to), value_ptr_at(from));
        key_policy_.destroy(key_ptr_at(from));
        value_policy_.destroy(value_ptr_at(from));
    }

    KeyPolicy                key_policy_{};
    ValuePolicy              value_policy_{};
    ComparePolicy            compare_{};
    std::byte*               storage_      = nullptr;
    std::size_t              capacity_     = 0u;
    std::size_t              size_         = 0u;
    std::size_t              entry_stride_ = 0u;
    std::size_t              value_offset_ = 0u;
    flat_map_policy          map_policy_   = flat_map_policy::none;
    flat_map_storage_kind    storage_kind_ = flat_map_storage_kind::external;
};

} // namespace memkit::detail
