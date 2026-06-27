#pragma once

#include "../config.hpp"
#include "../status.hpp"
#include "element_policy.hpp"
#include "growable_storage.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

namespace memkit::detail {

enum class linear_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline linear_storage_kind operator|(
    linear_storage_kind a,
    linear_storage_kind b
) noexcept
{
    return static_cast<linear_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

enum class growable_policy : std::uint8_t {
    none     = 0,
    growable = 1u << 0,
};

[[nodiscard]] inline growable_policy operator|(
    growable_policy a,
    growable_policy b
) noexcept
{
    return static_cast<growable_policy>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

[[nodiscard]] inline bool has(growable_policy flags, growable_policy bit) noexcept
{
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(bit)) != 0u;
}

template<typename Policy>
class vector_core {
public:
    vector_core() = default;

    [[nodiscard]] status init(
        Policy policy,
        std::byte* storage,
        std::size_t capacity,
        growable_policy grow_flags = growable_policy::none
    ) noexcept
    {
        if (storage == nullptr || capacity == 0u || policy.elem_size() == 0u) {
            return status::invalid;
        }

        policy_       = policy;
        storage_      = storage;
        capacity_     = capacity;
        size_         = 0u;
        grow_flags_   = grow_flags;
        return status::ok;
    }

    void reset_state() noexcept
    {
        policy_       = Policy{};
        storage_      = nullptr;
        capacity_     = 0u;
        size_         = 0u;
        grow_flags_   = growable_policy::none;
        storage_kind_ = linear_storage_kind::external;
        grow_alloc_   = {};
    }

    void set_storage_kind(linear_storage_kind kind) noexcept { storage_kind_ = kind; }
    void set_grow_alloc(grow_alloc alloc) noexcept { grow_alloc_ = alloc; }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }
    [[nodiscard]] growable_policy grow_flags() const noexcept { return grow_flags_; }
    [[nodiscard]] linear_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] std::byte* storage() const noexcept { return storage_; }
    [[nodiscard]] const Policy& policy() const noexcept { return policy_; }

    void clear() noexcept
    {
        if (size_ == 0u) {
            return;
        }

        if (policy_.needs_destroy_on_clear()) {
            for (std::size_t i = 0u; i < size_; ++i) {
                policy_.destroy(slot_at(i));
            }
        }

        size_ = 0u;
    }

    [[nodiscard]] status push_back(
        const void* elem,
        bool use_move = false
    ) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }

        if (size_ == capacity_) {
            const status grow_st = grow(size_ + 1u);
            if (grow_st != status::ok) {
                return grow_st;
            }
        }

        void* const slot = slot_at(size_);

        if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
            (void)use_move;
            const status copy_status = policy_.copy_construct(slot, elem);
            if (copy_status != status::ok) {
                return copy_status;
            }
        } else if (use_move) {
            policy_.move_construct(slot, const_cast<void*>(elem));
        } else {
            policy_.copy_construct(slot, elem);
        }

        ++size_;
        return status::ok;
    }

    [[nodiscard]] status pop_back(void* out_elem) noexcept
    {
        if (empty()) {
            return status::empty;
        }

        --size_;
        void* const slot = slot_at(size_);

        if (out_elem != nullptr) {
            if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
                const status copy_status = policy_.copy_construct(out_elem, slot);
                if (copy_status != status::ok) {
                    ++size_;
                    return copy_status;
                }
            } else {
                policy_.move_construct(out_elem, slot);
            }
        } else if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(slot);
        }

        if constexpr (!std::is_same_v<Policy, runtime_element_policy>) {
            if (out_elem != nullptr && policy_.needs_destroy_on_clear()) {
                policy_.destroy(slot);
            }
        }

        return status::ok;
    }

    [[nodiscard]] status peek_front(void* out_elem) const noexcept
    {
        return peek_at(0u, out_elem);
    }

    [[nodiscard]] status peek_back(void* out_elem) const noexcept
    {
        if (out_elem == nullptr) {
            return status::null_ptr;
        }
        if (empty()) {
            return status::empty;
        }

        std::memcpy(out_elem, slot_at(size_ - 1u), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] status peek_at(std::size_t index, void* out_elem) const noexcept
    {
        if (out_elem == nullptr) {
            return status::null_ptr;
        }
        if (index >= size_) {
            return status::invalid;
        }

        std::memcpy(out_elem, slot_at(index), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] status set_at(std::size_t index, const void* elem) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }
        if (index >= size_) {
            return status::invalid;
        }

        if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
            return policy_.copy_construct(slot_at(index), elem);
        }

        policy_.copy_assign(slot_at(index), elem);
        return status::ok;
    }

    [[nodiscard]] void* at(std::size_t index) const noexcept
    {
        return index < size_ ? slot_at(index) : nullptr;
    }

    template<typename VisitFn>
    [[nodiscard]] status foreach(VisitFn&& visit) const
    {
        for (std::size_t i = 0u; i < size_; ++i) {
            const status s = visit(slot_at(i), i);
            if (s != status::ok) {
                return s;
            }
        }
        return status::ok;
    }

    [[nodiscard]] status reserve(std::size_t min_capacity) noexcept
    {
        if (min_capacity <= capacity_) {
            return status::ok;
        }
        if (!has(grow_flags_, growable_policy::growable)) {
            return status::full;
        }
        return grow(min_capacity);
    }

    [[nodiscard]] status grow(std::size_t min_capacity) noexcept
    {
        if (!has(grow_flags_, growable_policy::growable)) {
            return status::full;
        }

        const std::size_t new_capacity = grow_capacity(capacity_, min_capacity);
        if (new_capacity < min_capacity) {
            return status::oom;
        }

        return reallocate(new_capacity);
    }

private:
    [[nodiscard]] void* slot_at(std::size_t index) const noexcept
    {
        return storage_ + index * policy_.elem_size();
    }

    [[nodiscard]] status copy_range(
        std::byte* dst,
        const std::byte* src,
        std::size_t count
    ) const
    {
        for (std::size_t i = 0u; i < count; ++i) {
            if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
                const status st = policy_.copy_construct(
                    dst + i * policy_.elem_size(),
                    src + i * policy_.elem_size()
                );
                if (st != status::ok) {
                    return st;
                }
            } else {
                policy_.move_construct(
                    dst + i * policy_.elem_size(),
                    const_cast<std::byte*>(src + i * policy_.elem_size())
                );
                policy_.destroy(const_cast<std::byte*>(src + i * policy_.elem_size()));
            }
        }
        return status::ok;
    }

    [[nodiscard]] status reallocate(std::size_t new_capacity) noexcept
    {
        if (new_capacity < size_) {
            return status::invalid;
        }

        const std::size_t bytes = new_capacity * policy_.elem_size();
        const std::size_t alignment = policy_.alignment();
        void* new_ptr = nullptr;

#if MEMKIT_ALLOW_HEAP
        const bool is_heap =
            (static_cast<std::uint8_t>(storage_kind_) &
             static_cast<std::uint8_t>(linear_storage_kind::heap)) != 0u;

        if (is_heap) {
            if (storage_ != nullptr) {
                new_ptr = std::realloc(storage_, bytes);
            } else {
                new_ptr = std::malloc(bytes);
            }
            if (new_ptr == nullptr) {
                return status::oom;
            }

            storage_  = static_cast<std::byte*>(new_ptr);
            capacity_ = new_capacity;
            return status::ok;
        } else
#endif
        if (grow_alloc_.allocate != nullptr) {
            const status st = grow_alloc_.allocate(
                grow_alloc_.ctx,
                bytes,
                alignment,
                &new_ptr
            );
            if (st != status::ok) {
                return st;
            }
        } else {
#if MEMKIT_ALLOW_HEAP
            new_ptr = std::malloc(bytes);
            if (new_ptr == nullptr) {
                return status::oom;
            }
            storage_kind_ = linear_storage_kind::owns | linear_storage_kind::heap;
#else
            return status::unsupported;
#endif
        }

        if (size_ > 0u && storage_ != nullptr && new_ptr != storage_) {
            const status copy_st = copy_range(
                static_cast<std::byte*>(new_ptr),
                storage_,
                size_
            );
            if (copy_st != status::ok) {
                return copy_st;
            }
        }

#if MEMKIT_ALLOW_HEAP
        if ((static_cast<std::uint8_t>(storage_kind_) &
             static_cast<std::uint8_t>(linear_storage_kind::heap)) != 0u &&
            storage_ != nullptr &&
            new_ptr != storage_) {
            std::free(storage_);
        }
#endif

        storage_  = static_cast<std::byte*>(new_ptr);
        capacity_ = new_capacity;
        return status::ok;
    }

    Policy                policy_{};
    std::byte*            storage_      = nullptr;
    std::size_t           capacity_     = 0u;
    std::size_t           size_         = 0u;
    growable_policy       grow_flags_   = growable_policy::none;
    linear_storage_kind   storage_kind_ = linear_storage_kind::external;
    grow_alloc            grow_alloc_{};
};

} // namespace memkit::detail
