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

enum class pqueue_policy : std::uint8_t {
    none     = 0u,
    growable = 1u << 0u,
};

enum class pqueue_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline pqueue_storage_kind operator|(
    pqueue_storage_kind a,
    pqueue_storage_kind b
) noexcept
{
    return static_cast<pqueue_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

template<typename Policy, typename Compare, typename T = void>
class pqueue_core {
public:
    pqueue_core() = default;

    [[nodiscard]] status init(
        Policy policy,
        Compare compare,
        std::byte* storage,
        std::size_t capacity,
        pqueue_policy queue_policy = pqueue_policy::none
    ) noexcept
    {
        if (storage == nullptr || capacity == 0u) {
            return status::invalid;
        }

        policy_       = policy;
        compare_      = std::move(compare);
        storage_      = storage;
        capacity_     = capacity;
        size_         = 0u;
        queue_policy_ = queue_policy;
        storage_kind_ = pqueue_storage_kind::external;
        return status::ok;
    }

    void reset_state() noexcept
    {
        policy_       = Policy{};
        compare_      = Compare{};
        storage_      = nullptr;
        capacity_     = 0u;
        size_         = 0u;
        queue_policy_ = pqueue_policy::none;
        storage_kind_ = pqueue_storage_kind::external;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }
    [[nodiscard]] pqueue_policy flags() const noexcept { return queue_policy_; }
    [[nodiscard]] pqueue_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] const Policy& policy() const noexcept { return policy_; }
    [[nodiscard]] std::byte* storage() const noexcept { return storage_; }

    void set_storage_kind(pqueue_storage_kind kind) noexcept { storage_kind_ = kind; }
    void set_capacity(std::size_t capacity) noexcept { capacity_ = capacity; }
    void set_storage(std::byte* storage, std::size_t capacity) noexcept
    {
        storage_  = storage;
        capacity_ = capacity;
    }

    [[nodiscard]] const Compare& compare() const noexcept { return compare_; }

    [[nodiscard]] bool full() const noexcept
    {
        if ((static_cast<unsigned>(queue_policy_) &
             static_cast<unsigned>(pqueue_policy::growable)) != 0u) {
            return false;
        }
        return size_ >= capacity_;
    }

    void clear() noexcept
    {
        if (policy_.needs_destroy_on_clear()) {
            for (std::size_t i = 0u; i < size_; ++i) {
                policy_.destroy(elem_at(i));
            }
        }
        size_ = 0u;
    }

    [[nodiscard]] status push(const void* value) noexcept
    {
        if (value == nullptr) {
            return status::null_ptr;
        }

        if (full()) {
            return status::full;
        }

        void* const slot = elem_at(size_);
        if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
            const status copy_st = policy_.copy_construct(slot, value);
            if (!ok(copy_st)) {
                return copy_st;
            }
        } else {
            policy_.copy_construct(slot, value);
        }

        ++size_;
        return sift_up(size_ - 1u);
    }

    [[nodiscard]] status pop(void* out = nullptr) noexcept
    {
        if (empty()) {
            return status::empty;
        }

        void* const root = elem_at(0u);

        if (out != nullptr) {
            if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
                const status copy_st = policy_.copy_construct(out, root);
                if (!ok(copy_st)) {
                    return copy_st;
                }
            } else {
                policy_.copy_construct(out, root);
            }
        } else if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(root);
        }

        --size_;
        if (size_ == 0u) {
            return status::ok;
        }

        const std::size_t last_index = size_;
        if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
            const status move_st = policy_.copy_construct(root, elem_at(last_index));
            if (!ok(move_st)) {
                ++size_;
                return move_st;
            }
        } else {
            policy_.move_construct(root, elem_at(last_index));
        }

        if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(elem_at(last_index));
        }

        return sift_down(0u);
    }

    [[nodiscard]] status peek(void* out) const noexcept
    {
        if (out == nullptr) {
            return status::null_ptr;
        }
        if (empty()) {
            return status::empty;
        }

        std::memcpy(out, elem_at(0u), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] void* top() noexcept
    {
        return empty() ? nullptr : elem_at(0u);
    }

    [[nodiscard]] const void* top() const noexcept
    {
        return empty() ? nullptr : elem_at(0u);
    }

    template<typename Visitor>
    [[nodiscard]] status for_each(Visitor&& visit) const
    {
        if (storage_ == nullptr) {
            return status::null_ptr;
        }

        for (std::size_t i = 0u; i < size_; ++i) {
            const void* slot = elem_at(i);
            if constexpr (std::is_same_v<std::invoke_result_t<Visitor, const void*, std::size_t>, status>) {
                const status st = visit(slot, i);
                if (!ok(st)) {
                    return st;
                }
            } else {
                visit(slot, i);
            }
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

    [[nodiscard]] bool less_than(const void* a, const void* b) const noexcept
    {
        if constexpr (std::is_same_v<Compare, runtime_compare_policy>) {
            return compare_.compare(a, b) < 0;
        } else {
            return compare_(*static_cast<const T*>(a), *static_cast<const T*>(b));
        }
    }

    [[nodiscard]] status swap_elements(std::size_t left, std::size_t right) noexcept
    {
        if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
            const std::size_t elem_size = policy_.elem_size();
            alignas(std::max_align_t) unsigned char temp[256];
            if (elem_size > sizeof(temp)) {
                return status::invalid;
            }
            const status to_temp = policy_.copy_construct(temp, elem_at(left));
            if (!ok(to_temp)) {
                return to_temp;
            }
            const status to_left = policy_.copy_construct(elem_at(left), elem_at(right));
            if (!ok(to_left)) {
                return to_left;
            }
            return policy_.copy_construct(elem_at(right), temp);
        } else {
            T temp{};
            policy_.move_construct(&temp, elem_at(left));
            policy_.move_construct(elem_at(left), elem_at(right));
            policy_.move_construct(elem_at(right), &temp);
            if (policy_.needs_destroy_on_clear()) {
                policy_.destroy(&temp);
            }
            return status::ok;
        }
    }

    [[nodiscard]] status sift_up(std::size_t index) noexcept
    {
        while (index > 0u) {
            const std::size_t parent = (index - 1u) / 2u;
            if (!less_than(elem_at(index), elem_at(parent))) {
                break;
            }

            const status swap_st = swap_elements(index, parent);
            if (!ok(swap_st)) {
                return swap_st;
            }
            index = parent;
        }
        return status::ok;
    }

    [[nodiscard]] status sift_down(std::size_t index) noexcept
    {
        while (true) {
            std::size_t smallest = index;
            const std::size_t left = (index * 2u) + 1u;
            const std::size_t right = left + 1u;

            if (left < size_ && less_than(elem_at(left), elem_at(smallest))) {
                smallest = left;
            }
            if (right < size_ && less_than(elem_at(right), elem_at(smallest))) {
                smallest = right;
            }
            if (smallest == index) {
                break;
            }

            const status swap_st = swap_elements(index, smallest);
            if (!ok(swap_st)) {
                return swap_st;
            }
            index = smallest;
        }
        return status::ok;
    }

    Policy               policy_{};
    Compare              compare_{};
    std::byte*           storage_      = nullptr;
    std::size_t          capacity_     = 0u;
    std::size_t          size_         = 0u;
    pqueue_policy        queue_policy_ = pqueue_policy::none;
    pqueue_storage_kind  storage_kind_ = pqueue_storage_kind::external;
};

} // namespace memkit::detail
