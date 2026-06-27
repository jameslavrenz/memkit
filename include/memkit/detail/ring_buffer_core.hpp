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

enum class ring_buffer_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline ring_buffer_storage_kind operator|(
    ring_buffer_storage_kind a,
    ring_buffer_storage_kind b
) noexcept
{
    return static_cast<ring_buffer_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

enum class ring_buffer_policy : std::uint8_t {
    none              = 0,
    growable          = 1u << 0,
    overwrite_on_full = 1u << 1,
};

[[nodiscard]] inline ring_buffer_policy operator|(
    ring_buffer_policy a,
    ring_buffer_policy b
) noexcept
{
    return static_cast<ring_buffer_policy>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

[[nodiscard]] inline ring_buffer_policy operator&(
    ring_buffer_policy a,
    ring_buffer_policy b
) noexcept
{
    return static_cast<ring_buffer_policy>(
        static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b)
    );
}

[[nodiscard]] inline bool has(ring_buffer_policy flags, ring_buffer_policy bit) noexcept
{
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(bit)) != 0u;
}

template<typename Policy>
class ring_buffer_core {
public:
    ring_buffer_core() = default;

    [[nodiscard]] status init(
        Policy policy,
        std::byte* storage,
        std::size_t capacity,
        ring_buffer_policy buffer_flags = ring_buffer_policy::none
    ) noexcept
    {
        if (capacity == 0u || policy.elem_size() == 0u) {
            return status::invalid;
        }
        if (storage == nullptr) {
            return status::null_ptr;
        }

        policy_       = policy;
        storage_      = storage;
        capacity_     = capacity;
        head_         = 0u;
        tail_         = 0u;
        count_        = 0u;
        buffer_flags_ = buffer_flags;
        return status::ok;
    }

    void reset_state() noexcept
    {
        policy_       = Policy{};
        storage_      = nullptr;
        capacity_     = 0u;
        head_         = 0u;
        tail_         = 0u;
        count_        = 0u;
        buffer_flags_ = ring_buffer_policy::none;
        storage_kind_ = ring_buffer_storage_kind::external;
        grow_alloc_   = {};
    }

    void set_storage_kind(ring_buffer_storage_kind kind) noexcept { storage_kind_ = kind; }
    void set_grow_alloc(grow_alloc alloc) noexcept { grow_alloc_ = alloc; }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0u; }

    [[nodiscard]] bool full() const noexcept
    {
        if (has(buffer_flags_, ring_buffer_policy::growable)) {
            return false;
        }
        return count_ >= capacity_;
    }

    [[nodiscard]] ring_buffer_policy flags() const noexcept { return buffer_flags_; }
    [[nodiscard]] ring_buffer_policy grow_flags() const noexcept { return buffer_flags_; }
    [[nodiscard]] ring_buffer_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] std::byte* storage() const noexcept { return storage_; }
    [[nodiscard]] const Policy& policy() const noexcept { return policy_; }

    void clear() noexcept
    {
        if (count_ == 0u) {
            return;
        }

        if (policy_.needs_destroy_on_clear()) {
            for (std::size_t i = 0u; i < count_; ++i) {
                policy_.destroy(slot_at(logical_index(i)));
            }
        }

        head_  = 0u;
        tail_  = 0u;
        count_ = 0u;
    }

    [[nodiscard]] status push_back(
        const void* elem,
        bool allow_overwrite = false,
        bool use_move = false
    ) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }

        if (capacity_ == 0u) {
            return status::invalid;
        }

        if (count_ == capacity_ &&
            !has(buffer_flags_, ring_buffer_policy::growable) &&
            !allow_overwrite &&
            !has(buffer_flags_, ring_buffer_policy::overwrite_on_full)) {
            return status::full;
        }

        if (count_ >= capacity_) {
            if (has(buffer_flags_, ring_buffer_policy::growable)) {
                const status room_st = ensure_room();
                if (room_st != status::ok) {
                    return room_st;
                }
            } else if (allow_overwrite ||
                       has(buffer_flags_, ring_buffer_policy::overwrite_on_full)) {
                if (policy_.needs_destroy_on_clear()) {
                    policy_.destroy(slot_at(tail_));
                }

                tail_ = advance(tail_, 1u);
                --count_;
            } else {
                return status::full;
            }
        }

        void* const slot = slot_at(head_);

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

        head_ = advance(head_, 1u);
        ++count_;
        return status::ok;
    }

    [[nodiscard]] status push_front(const void* elem, bool use_move = false) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }

        if (count_ >= capacity_) {
            if (has(buffer_flags_, ring_buffer_policy::growable)) {
                const status room_st = ensure_room();
                if (room_st != status::ok) {
                    return room_st;
                }
            } else if (has(buffer_flags_, ring_buffer_policy::overwrite_on_full)) {
                if (policy_.needs_destroy_on_clear()) {
                    const std::size_t back = (head_ + capacity_ - 1u) % capacity_;
                    policy_.destroy(slot_at(back));
                }

                head_ = (head_ + capacity_ - 1u) % capacity_;
                --count_;
            } else {
                return status::full;
            }
        }

        tail_ = (tail_ + capacity_ - 1u) % capacity_;
        void* const slot = slot_at(tail_);

        if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
            (void)use_move;
            const status copy_status = policy_.copy_construct(slot, elem);
            if (copy_status != status::ok) {
                tail_ = advance(tail_, 1u);
                return copy_status;
            }
        } else if (use_move) {
            policy_.move_construct(slot, const_cast<void*>(elem));
        } else {
            policy_.copy_construct(slot, elem);
        }

        ++count_;
        return status::ok;
    }

    [[nodiscard]] status pop_front(void* out_elem, bool move_out = false) noexcept
    {
        if (empty()) {
            return status::empty;
        }

        void* const slot = slot_at(tail_);
        const status st = extract_slot(slot, out_elem, move_out);
        if (st != status::ok) {
            return st;
        }

        tail_ = advance(tail_, 1u);
        --count_;
        return status::ok;
    }

    [[nodiscard]] status pop_back(void* out_elem, bool move_out = false) noexcept
    {
        if (empty()) {
            return status::empty;
        }

        head_ = (head_ + capacity_ - 1u) % capacity_;
        void* const slot = slot_at(head_);

        const status st = extract_slot(slot, out_elem, move_out);
        if (st != status::ok) {
            head_ = advance(head_, 1u);
            return st;
        }

        --count_;
        return status::ok;
    }

    [[nodiscard]] status peek_front(void* out_elem) const noexcept
    {
        if (out_elem == nullptr) {
            return status::null_ptr;
        }
        if (empty()) {
            return status::empty;
        }

        std::memcpy(out_elem, slot_at(tail_), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] status peek_back(void* out_elem) const noexcept
    {
        if (out_elem == nullptr) {
            return status::null_ptr;
        }
        if (empty()) {
            return status::empty;
        }

        const std::size_t index = (head_ + capacity_ - 1u) % capacity_;
        std::memcpy(out_elem, slot_at(index), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] status peek_at(std::size_t index, void* out_elem) const noexcept
    {
        if (out_elem == nullptr) {
            return status::null_ptr;
        }
        if (index >= count_) {
            return status::invalid;
        }

        std::memcpy(out_elem, slot_at(logical_index(index)), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] status set_at(std::size_t index, const void* elem) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }
        if (index >= count_) {
            return status::invalid;
        }

        if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
            return policy_.copy_construct(slot_at(logical_index(index)), elem);
        }

        policy_.copy_assign(slot_at(logical_index(index)), elem);
        return status::ok;
    }

    [[nodiscard]] void* logical_slot(std::size_t index) const noexcept
    {
        return index < count_ ? slot_at(logical_index(index)) : nullptr;
    }

    [[nodiscard]] void* front() const noexcept
    {
        return empty() ? nullptr : slot_at(tail_);
    }

    [[nodiscard]] void* back() const noexcept
    {
        if (empty()) {
            return nullptr;
        }
        const std::size_t index = (head_ + capacity_ - 1u) % capacity_;
        return slot_at(index);
    }

    [[nodiscard]] status reserve(std::size_t min_capacity) noexcept
    {
        if (min_capacity <= capacity_) {
            return status::ok;
        }
        if (!has(buffer_flags_, ring_buffer_policy::growable)) {
            return status::full;
        }

        const std::size_t new_capacity = grow_capacity(capacity_, min_capacity);
        return relinearize(new_capacity);
    }

    template<typename VisitFn>
    [[nodiscard]] status foreach(VisitFn&& visit) const
    {
        for (std::size_t i = 0u; i < count_; ++i) {
            const status s = visit(slot_at(logical_index(i)), i);
            if (s != status::ok) {
                return s;
            }
        }
        return status::ok;
    }

    [[nodiscard]] std::size_t readable_contiguous(const void** out_ptr) const noexcept
    {
        if (out_ptr != nullptr) {
            *out_ptr = nullptr;
        }
        if (empty() || out_ptr == nullptr) {
            return 0u;
        }

        *out_ptr = slot_at(tail_);

        if (tail_ < head_) {
            return head_ - tail_;
        }

        return count_;
    }

    [[nodiscard]] std::size_t writable_contiguous(void** out_ptr) noexcept
    {
        if (out_ptr != nullptr) {
            *out_ptr = nullptr;
        }
        if (out_ptr == nullptr) {
            return 0u;
        }

        if (count_ >= capacity_) {
            if (!has(buffer_flags_, ring_buffer_policy::growable)) {
                return 0u;
            }

            const status grow_st = ensure_room();
            if (grow_st != status::ok) {
                return 0u;
            }
        }

        *out_ptr = slot_at(head_);

        if (head_ >= tail_) {
            return capacity_ - count_;
        }

        return head_ - tail_;
    }

    void commit_read(std::size_t elem_count) noexcept
    {
        if (elem_count == 0u || empty()) {
            return;
        }

        if (elem_count > count_) {
            elem_count = count_;
        }

        if (policy_.needs_destroy_on_clear()) {
            for (std::size_t i = 0u; i < elem_count; ++i) {
                policy_.destroy(slot_at((tail_ + i) % capacity_));
            }
        }

        tail_ = advance(tail_, elem_count);
        count_ -= elem_count;
    }

    void commit_write(std::size_t elem_count) noexcept
    {
        if (elem_count == 0u) {
            return;
        }

        const std::size_t free_slots = capacity_ - count_;
        if (elem_count > free_slots) {
            elem_count = free_slots;
        }

        head_ = advance(head_, elem_count);
        count_ += elem_count;
    }

private:
    [[nodiscard]] status extract_slot(
        void* slot,
        void* out_elem,
        bool move_out
    ) noexcept
    {
        if (out_elem != nullptr) {
            if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
                const status copy_status = policy_.copy_construct(out_elem, slot);
                if (copy_status != status::ok) {
                    return copy_status;
                }
            } else if (move_out) {
                policy_.move_construct(out_elem, slot);
                if (policy_.needs_destroy_on_clear()) {
                    policy_.destroy(slot);
                }
            } else {
                policy_.copy_construct(out_elem, slot);
            }
        } else if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(slot);
        }

        return status::ok;
    }

    [[nodiscard]] std::size_t logical_index(std::size_t logical) const noexcept
    {
        return (tail_ + logical) % capacity_;
    }

    [[nodiscard]] std::size_t advance(std::size_t index, std::size_t delta) const noexcept
    {
        return (index + delta) % capacity_;
    }

    [[nodiscard]] void* slot_at(std::size_t physical_index) const noexcept
    {
        return storage_ + physical_index * policy_.elem_size();
    }

    [[nodiscard]] status ensure_room() noexcept
    {
        if (count_ < capacity_) {
            return status::ok;
        }

        if (!has(buffer_flags_, ring_buffer_policy::growable)) {
            return status::full;
        }

        const std::size_t new_capacity = grow_capacity(capacity_, count_ + 1u);
        if (new_capacity <= capacity_) {
            return status::full;
        }

        return relinearize(new_capacity);
    }

    [[nodiscard]] status relinearize(std::size_t new_capacity) noexcept
    {
        if (new_capacity < count_) {
            return status::invalid;
        }

        const std::size_t bytes = new_capacity * policy_.elem_size();
        const std::size_t alignment = policy_.alignment();
        void* new_ptr = nullptr;

#if MEMKIT_ALLOW_HEAP
        const bool is_heap =
            (static_cast<std::uint8_t>(storage_kind_) &
             static_cast<std::uint8_t>(ring_buffer_storage_kind::heap)) != 0u;

        if (is_heap) {
            new_ptr = std::malloc(bytes);
            if (new_ptr == nullptr) {
                return status::oom;
            }
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
            storage_kind_ = ring_buffer_storage_kind::owns | ring_buffer_storage_kind::heap;
#else
            return status::unsupported;
#endif
        }

        std::byte* const new_storage = static_cast<std::byte*>(new_ptr);

        for (std::size_t i = 0u; i < count_; ++i) {
            void* const dst = new_storage + i * policy_.elem_size();
            void* const src = slot_at((tail_ + i) % capacity_);

            if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
                std::memcpy(dst, src, policy_.elem_size());
            } else {
                policy_.move_construct(dst, src);
                policy_.destroy(src);
            }
        }

#if MEMKIT_ALLOW_HEAP
        if (is_heap && storage_ != nullptr) {
            std::free(storage_);
        }
#endif

        storage_  = new_storage;
        capacity_ = new_capacity;
        head_     = count_;
        tail_     = 0u;
        return status::ok;
    }

    Policy                   policy_{};
    std::byte*               storage_      = nullptr;
    std::size_t              capacity_     = 0u;
    std::size_t              head_         = 0u;
    std::size_t              tail_         = 0u;
    std::size_t              count_        = 0u;
    ring_buffer_policy       buffer_flags_ = ring_buffer_policy::none;
    ring_buffer_storage_kind storage_kind_ = ring_buffer_storage_kind::external;
    grow_alloc               grow_alloc_{};
};

} // namespace memkit::detail
