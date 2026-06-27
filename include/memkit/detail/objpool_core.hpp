#pragma once

#include "../status.hpp"
#include "element_policy.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace memkit::detail {

enum class objpool_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline objpool_storage_kind operator|(
    objpool_storage_kind a,
    objpool_storage_kind b
) noexcept
{
    return static_cast<objpool_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

template<typename Policy>
class objpool_core {
public:
    objpool_core() = default;

    [[nodiscard]] static constexpr std::size_t used_bits_bytes(std::size_t capacity) noexcept
    {
        return (capacity + 7u) / 8u;
    }

    [[nodiscard]] static constexpr std::size_t free_stack_bytes(std::size_t capacity) noexcept
    {
        return capacity * sizeof(std::uint32_t);
    }

    [[nodiscard]] status init(
        Policy policy,
        std::byte* storage,
        std::uint32_t* free_stack,
        std::byte* used_bits,
        std::size_t capacity
    ) noexcept
    {
        if (storage == nullptr || free_stack == nullptr || used_bits == nullptr || capacity == 0u) {
            return status::invalid;
        }

        policy_       = policy;
        storage_      = storage;
        free_stack_   = free_stack;
        used_bits_    = used_bits;
        capacity_     = capacity;
        storage_kind_ = objpool_storage_kind::external;
        return reset_free_list();
    }

    void reset_state() noexcept
    {
        policy_       = Policy{};
        storage_      = nullptr;
        free_stack_   = nullptr;
        used_bits_    = nullptr;
        capacity_     = 0u;
        used_count_   = 0u;
        free_count_   = 0u;
        storage_kind_ = objpool_storage_kind::external;
    }

    [[nodiscard]] std::size_t size() const noexcept { return used_count_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t available() const noexcept { return free_count_; }
    [[nodiscard]] bool empty() const noexcept { return used_count_ == 0u; }
    [[nodiscard]] bool full() const noexcept { return free_count_ == 0u; }
    [[nodiscard]] objpool_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] const Policy& policy() const noexcept { return policy_; }
    [[nodiscard]] std::byte* storage() const noexcept { return storage_; }
    [[nodiscard]] std::uint32_t* free_stack() const noexcept { return free_stack_; }
    [[nodiscard]] std::byte* used_bits() const noexcept { return used_bits_; }

    void set_storage_kind(objpool_storage_kind kind) noexcept { storage_kind_ = kind; }

    void clear() noexcept
    {
        if (used_count_ == 0u) {
            return;
        }

        if (policy_.needs_destroy_on_clear()) {
            for (std::size_t i = 0u; i < capacity_; ++i) {
                if (bit_test(i)) {
                    policy_.destroy(elem_at(i));
                }
            }
        }

        (void)reset_free_list();
    }

    [[nodiscard]] status alloc(void** out_elem) noexcept
    {
        if (out_elem == nullptr) {
            return status::null_ptr;
        }
        if (free_count_ == 0u) {
            return status::full;
        }

        const std::uint32_t index = free_stack_[--free_count_];
        if (index >= capacity_) {
            ++free_count_;
            return status::invalid;
        }

        bit_set(index);
        ++used_count_;
        *out_elem = elem_at(index);
        return status::ok;
    }

    [[nodiscard]] status alloc_copy(const void* src, void** out_elem) noexcept
    {
        void* elem = nullptr;
        const status alloc_st = alloc(&elem);
        if (!ok(alloc_st)) {
            return alloc_st;
        }

        if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
            const status copy_st = policy_.copy_construct(elem, src);
            if (!ok(copy_st)) {
                (void)free(elem);
                return copy_st;
            }
        } else {
            policy_.copy_construct(elem, src);
        }

        *out_elem = elem;
        return status::ok;
    }

    [[nodiscard]] status index(const void* elem, std::size_t& out_index) const noexcept
    {
        if (elem == nullptr || storage_ == nullptr) {
            return status::null_ptr;
        }

        const auto base = reinterpret_cast<std::uintptr_t>(storage_);
        const auto address = reinterpret_cast<std::uintptr_t>(elem);

        if (address < base) {
            return status::not_found;
        }

        const auto offset = address - base;
        if (offset % policy_.elem_size() != 0u) {
            return status::not_found;
        }

        const std::size_t idx = static_cast<std::size_t>(offset / policy_.elem_size());
        if (idx >= capacity_) {
            return status::not_found;
        }

        out_index = idx;
        return status::ok;
    }

    [[nodiscard]] bool contains(const void* elem) const noexcept
    {
        std::size_t idx = 0u;
        if (!ok(index(elem, idx))) {
            return false;
        }
        return bit_test(idx);
    }

    [[nodiscard]] status free(void* elem) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }

        std::size_t idx = 0u;
        const status index_st = index(elem, idx);
        if (!ok(index_st)) {
            return index_st;
        }

        if (!bit_test(idx)) {
            return status::not_found;
        }

        if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(elem);
        }

        bit_clear(idx);
        free_stack_[free_count_++] = static_cast<std::uint32_t>(idx);
        --used_count_;
        return status::ok;
    }

    template<typename Visitor>
    [[nodiscard]] status for_each(Visitor&& visit) const
    {
        if (storage_ == nullptr) {
            return status::null_ptr;
        }

        std::size_t visit_index = 0u;
        for (std::size_t i = 0u; i < capacity_; ++i) {
            if (!bit_test(i)) {
                continue;
            }

            const void* elem = elem_at(i);
            if constexpr (std::is_same_v<std::invoke_result_t<Visitor, const void*, std::size_t>, status>) {
                const status st = visit(elem, visit_index);
                if (!ok(st)) {
                    return st;
                }
            } else {
                visit(elem, visit_index);
            }
            ++visit_index;
        }

        return status::ok;
    }

private:
    [[nodiscard]] void* elem_at(std::size_t index) noexcept
    {
        return storage_ + index * policy_.elem_size();
    }

    [[nodiscard]] const void* elem_at(std::size_t index) const noexcept
    {
        return storage_ + index * policy_.elem_size();
    }

    [[nodiscard]] bool bit_test(std::size_t index) const noexcept
    {
        return (used_bits_[index / 8u] & static_cast<std::byte>(1u << (index % 8u))) != std::byte{0};
    }

    void bit_set(std::size_t index) noexcept
    {
        used_bits_[index / 8u] |= static_cast<std::byte>(1u << (index % 8u));
    }

    void bit_clear(std::size_t index) noexcept
    {
        used_bits_[index / 8u] &= static_cast<std::byte>(~(1u << (index % 8u)));
    }

    [[nodiscard]] status reset_free_list() noexcept
    {
        if (storage_ == nullptr || free_stack_ == nullptr || used_bits_ == nullptr) {
            return status::invalid;
        }

        std::memset(used_bits_, 0, used_bits_bytes(capacity_));
        used_count_ = 0u;
        free_count_ = capacity_;

        for (std::size_t i = 0u; i < capacity_; ++i) {
            free_stack_[i] = static_cast<std::uint32_t>(capacity_ - 1u - i);
        }

        return status::ok;
    }

    Policy                policy_{};
    std::byte*            storage_      = nullptr;
    std::uint32_t*        free_stack_   = nullptr;
    std::byte*            used_bits_    = nullptr;
    std::size_t           capacity_     = 0u;
    std::size_t           used_count_   = 0u;
    std::size_t           free_count_   = 0u;
    objpool_storage_kind  storage_kind_ = objpool_storage_kind::external;
};

} // namespace memkit::detail
