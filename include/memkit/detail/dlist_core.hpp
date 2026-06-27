#pragma once

#include "../status.hpp"
#include "element_policy.hpp"
#include "utility.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

namespace memkit::detail {

enum class dlist_policy : std::uint8_t {
    none         = 0u,
    fixed_pool   = 1u << 0u,
    heap_dynamic = 1u << 1u,
};

enum class dlist_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline dlist_storage_kind operator|(
    dlist_storage_kind a,
    dlist_storage_kind b
) noexcept
{
    return static_cast<dlist_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

struct dlist_node_header {
    dlist_node_header* prev = nullptr;
    dlist_node_header* next = nullptr;
};

template<typename Policy>
class dlist_core {
public:
    dlist_core() = default;

    [[nodiscard]] static std::size_t node_stride(std::size_t elem_size) noexcept
    {
        const std::size_t header = align_up(sizeof(dlist_node_header), alignof(dlist_node_header));
        return header + elem_size;
    }

    [[nodiscard]] static std::size_t pool_bytes(std::size_t node_capacity, std::size_t elem_size) noexcept
    {
        return node_stride(elem_size) * node_capacity;
    }

    [[nodiscard]] status init(
        Policy policy,
        std::byte* node_pool,
        std::size_t node_capacity,
        dlist_policy list_flags = dlist_policy::fixed_pool
    ) noexcept
    {
        if (node_pool == nullptr || node_capacity == 0u) {
            return status::invalid;
        }

        policy_        = policy;
        node_pool_     = node_pool;
        node_capacity_ = node_capacity;
        head_          = nullptr;
        tail_          = nullptr;
        size_          = 0u;
        list_flags_    = list_flags;
        storage_kind_  = dlist_storage_kind::external;
        return init_free_list();
    }

    [[nodiscard]] status init_dynamic(Policy policy) noexcept
    {
        policy_        = policy;
        node_pool_     = nullptr;
        node_capacity_ = 0u;
        head_          = nullptr;
        tail_          = nullptr;
        free_list_     = nullptr;
        size_          = 0u;
        list_flags_    = dlist_policy::heap_dynamic;
        storage_kind_  = dlist_storage_kind::external;
        return status::ok;
    }

    void reset_state() noexcept
    {
        policy_        = Policy{};
        head_          = nullptr;
        tail_          = nullptr;
        free_list_     = nullptr;
        size_          = 0u;
        node_capacity_ = 0u;
        node_pool_     = nullptr;
        list_flags_    = dlist_policy::none;
        storage_kind_  = dlist_storage_kind::external;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept
    {
        if ((static_cast<unsigned>(list_flags_) &
             static_cast<unsigned>(dlist_policy::fixed_pool)) != 0u) {
            return node_capacity_;
        }
        return SIZE_MAX;
    }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }
    [[nodiscard]] dlist_policy flags() const noexcept { return list_flags_; }
    [[nodiscard]] dlist_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] const Policy& policy() const noexcept { return policy_; }
    [[nodiscard]] std::byte* node_pool() const noexcept { return node_pool_; }
    [[nodiscard]] std::size_t node_stride() const noexcept { return node_stride(policy_.elem_size()); }

    void set_storage_kind(dlist_storage_kind kind) noexcept { storage_kind_ = kind; }

    [[nodiscard]] bool full() const noexcept
    {
        if ((static_cast<unsigned>(list_flags_) &
             static_cast<unsigned>(dlist_policy::fixed_pool)) == 0u) {
            return false;
        }
        return size_ >= node_capacity_;
    }

    void clear() noexcept
    {
        while (head_ != nullptr) {
            dlist_node_header* current = head_;
            unlink_node(current);
            destroy_node_value(current);
            release_node(current);
        }
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0u;
    }

    [[nodiscard]] status push_front(const void* value) noexcept { return insert_front(value); }
    [[nodiscard]] status push_back(const void* value) noexcept { return insert_back(value); }

    [[nodiscard]] status pop_front(void* out = nullptr) noexcept
    {
        if (empty()) {
            return status::empty;
        }
        return pop_node(head_, out);
    }

    [[nodiscard]] status pop_back(void* out = nullptr) noexcept
    {
        if (empty()) {
            return status::empty;
        }
        return pop_node(tail_, out);
    }

    [[nodiscard]] status peek_front(void* out) const noexcept
    {
        if (out == nullptr) {
            return status::null_ptr;
        }
        if (empty()) {
            return status::empty;
        }
        std::memcpy(out, node_data(head_), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] status peek_back(void* out) const noexcept
    {
        if (out == nullptr) {
            return status::null_ptr;
        }
        if (empty()) {
            return status::empty;
        }
        std::memcpy(out, node_data(tail_), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] status peek_at(std::size_t index, void* out) const noexcept
    {
        if (out == nullptr) {
            return status::null_ptr;
        }

        const dlist_node_header* current = node_at_const(index);
        if (current == nullptr) {
            return status::invalid;
        }

        std::memcpy(out, node_data(current), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] status insert_at(std::size_t index, const void* value) noexcept
    {
        if (value == nullptr) {
            return status::null_ptr;
        }
        if (index > size_) {
            return status::invalid;
        }
        if (index == 0u) {
            return insert_front(value);
        }
        if (index == size_) {
            return insert_back(value);
        }

        dlist_node_header* target = node_at(index);
        if (target == nullptr) {
            return status::invalid;
        }

        dlist_node_header* new_node = nullptr;
        const status make_st = make_node(value, &new_node);
        if (!ok(make_st)) {
            return make_st;
        }

        link_before(target, new_node);
        ++size_;
        return status::ok;
    }

    [[nodiscard]] status remove_at(std::size_t index, void* out = nullptr) noexcept
    {
        if (index >= size_) {
            return status::invalid;
        }
        if (index == 0u) {
            return pop_front(out);
        }
        if (index == size_ - 1u) {
            return pop_back(out);
        }

        dlist_node_header* target = node_at(index);
        if (target == nullptr) {
            return status::invalid;
        }
        return pop_node(target, out);
    }

    template<typename Predicate>
    [[nodiscard]] status remove_first(Predicate&& pred, void* out = nullptr) noexcept
    {
        std::size_t index = 0u;
        for (dlist_node_header* current = head_; current != nullptr; current = current->next) {
            if (pred(node_data(current))) {
                return remove_at(index, out);
            }
            ++index;
        }
        return status::not_found;
    }

    [[nodiscard]] void* front() noexcept
    {
        return empty() ? nullptr : node_data(head_);
    }

    [[nodiscard]] const void* front() const noexcept
    {
        return empty() ? nullptr : node_data(head_);
    }

    [[nodiscard]] void* back() noexcept
    {
        return empty() ? nullptr : node_data(tail_);
    }

    [[nodiscard]] const void* back() const noexcept
    {
        return empty() ? nullptr : node_data(tail_);
    }

    template<typename Visitor>
    [[nodiscard]] status for_each(Visitor&& visit) const
    {
        std::size_t index = 0u;
        for (const dlist_node_header* current = head_; current != nullptr; current = current->next) {
            const void* value = node_data(current);
            if constexpr (std::is_same_v<std::invoke_result_t<Visitor, const void*, std::size_t>, status>) {
                const status st = visit(value, index);
                if (!ok(st)) {
                    return st;
                }
            } else {
                visit(value, index);
            }
            ++index;
        }
        return status::ok;
    }

    template<typename Visitor>
    [[nodiscard]] status for_each_reverse(Visitor&& visit) const
    {
        if (size_ == 0u) {
            return status::ok;
        }

        std::size_t index = size_ - 1u;
        for (const dlist_node_header* current = tail_; current != nullptr; current = current->prev) {
            const void* value = node_data(current);
            if constexpr (std::is_same_v<std::invoke_result_t<Visitor, const void*, std::size_t>, status>) {
                const status st = visit(value, index);
                if (!ok(st)) {
                    return st;
                }
            } else {
                visit(value, index);
            }
            if (index == 0u) {
                break;
            }
            --index;
        }

        return status::ok;
    }

private:
    [[nodiscard]] status insert_front(const void* value) noexcept
    {
        dlist_node_header* new_node = nullptr;
        const status make_st = make_node(value, &new_node);
        if (!ok(make_st)) {
            return make_st;
        }

        link_front(new_node);
        ++size_;
        return status::ok;
    }

    [[nodiscard]] status insert_back(const void* value) noexcept
    {
        dlist_node_header* new_node = nullptr;
        const status make_st = make_node(value, &new_node);
        if (!ok(make_st)) {
            return make_st;
        }

        link_back(new_node);
        ++size_;
        return status::ok;
    }

    [[nodiscard]] dlist_node_header* pool_node_at(std::size_t index) noexcept
    {
        return reinterpret_cast<dlist_node_header*>(node_pool_ + index * node_stride());
    }

    [[nodiscard]] status init_free_list() noexcept
    {
        free_list_ = nullptr;
        for (std::size_t i = 0u; i < node_capacity_; ++i) {
            dlist_node_header* slot = pool_node_at(i);
            slot->prev = nullptr;
            slot->next = free_list_;
            free_list_ = slot;
        }
        return status::ok;
    }

    [[nodiscard]] status acquire_node(dlist_node_header** out_node) noexcept
    {
        if ((static_cast<unsigned>(list_flags_) &
             static_cast<unsigned>(dlist_policy::fixed_pool)) != 0u) {
            if (free_list_ == nullptr) {
                return status::full;
            }
            dlist_node_header* slot = free_list_;
            free_list_ = slot->next;
            slot->prev = nullptr;
            slot->next = nullptr;
            *out_node = slot;
            return status::ok;
        }

#if MEMKIT_ALLOW_HEAP
        void* memory = std::malloc(node_stride());
        if (memory == nullptr) {
            return status::oom;
        }
        dlist_node_header* slot = static_cast<dlist_node_header*>(memory);
        slot->prev = nullptr;
        slot->next = nullptr;
        *out_node = slot;
        return status::ok;
#else
        (void)out_node;
        return status::unsupported;
#endif
    }

    void release_node(dlist_node_header* slot) noexcept
    {
        if (slot == nullptr) {
            return;
        }

        if ((static_cast<unsigned>(list_flags_) &
             static_cast<unsigned>(dlist_policy::fixed_pool)) != 0u) {
            slot->prev = nullptr;
            slot->next = free_list_;
            free_list_ = slot;
            return;
        }

#if MEMKIT_ALLOW_HEAP
        std::free(slot);
#endif
    }

    [[nodiscard]] status make_node(const void* value, dlist_node_header** out_node) noexcept
    {
        dlist_node_header* slot = nullptr;
        const status acquire_st = acquire_node(&slot);
        if (!ok(acquire_st)) {
            return acquire_st;
        }

        if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
            const status copy_st = policy_.copy_construct(node_data(slot), value);
            if (!ok(copy_st)) {
                release_node(slot);
                return copy_st;
            }
        } else {
            policy_.copy_construct(node_data(slot), value);
        }

        *out_node = slot;
        return status::ok;
    }

    void destroy_node_value(dlist_node_header* slot) noexcept
    {
        if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(node_data(slot));
        }
    }

    void link_front(dlist_node_header* slot) noexcept
    {
        slot->prev = nullptr;
        slot->next = head_;
        if (head_ != nullptr) {
            head_->prev = slot;
        } else {
            tail_ = slot;
        }
        head_ = slot;
    }

    void link_back(dlist_node_header* slot) noexcept
    {
        slot->next = nullptr;
        slot->prev = tail_;
        if (tail_ != nullptr) {
            tail_->next = slot;
        } else {
            head_ = slot;
        }
        tail_ = slot;
    }

    void link_before(dlist_node_header* target, dlist_node_header* new_node) noexcept
    {
        new_node->next = target;
        new_node->prev = target->prev;
        if (target->prev != nullptr) {
            target->prev->next = new_node;
        } else {
            head_ = new_node;
        }
        target->prev = new_node;
    }

    void unlink_node(dlist_node_header* slot) noexcept
    {
        if (slot->prev != nullptr) {
            slot->prev->next = slot->next;
        } else {
            head_ = slot->next;
        }

        if (slot->next != nullptr) {
            slot->next->prev = slot->prev;
        } else {
            tail_ = slot->prev;
        }

        slot->prev = nullptr;
        slot->next = nullptr;
    }

    [[nodiscard]] status pop_node(dlist_node_header* slot, void* out) noexcept
    {
        if (out != nullptr) {
            if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
                const status copy_st = policy_.copy_construct(out, node_data(slot));
                if (!ok(copy_st)) {
                    return copy_st;
                }
            } else {
                policy_.copy_construct(out, node_data(slot));
            }
        } else if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(node_data(slot));
        }

        unlink_node(slot);
        release_node(slot);
        --size_;
        return status::ok;
    }

    [[nodiscard]] dlist_node_header* node_at(std::size_t index) noexcept
    {
        if (index >= size_) {
            return nullptr;
        }

        if (index <= size_ / 2u) {
            dlist_node_header* current = head_;
            for (std::size_t i = 0u; i < index; ++i) {
                current = current->next;
            }
            return current;
        }

        dlist_node_header* current = tail_;
        for (std::size_t i = size_ - 1u; i > index; --i) {
            current = current->prev;
        }
        return current;
    }

    [[nodiscard]] const dlist_node_header* node_at_const(std::size_t index) const noexcept
    {
        return const_cast<dlist_core*>(this)->node_at(index);
    }

    [[nodiscard]] void* node_data(dlist_node_header* node) const noexcept
    {
        const std::size_t header = align_up(sizeof(dlist_node_header), alignof(dlist_node_header));
        return reinterpret_cast<std::byte*>(node) + header;
    }

    [[nodiscard]] const void* node_data(const dlist_node_header* node) const noexcept
    {
        return node_data(const_cast<dlist_node_header*>(node));
    }

    Policy               policy_{};
    dlist_node_header*   head_          = nullptr;
    dlist_node_header*   tail_          = nullptr;
    dlist_node_header*   free_list_     = nullptr;
    std::size_t          size_          = 0u;
    std::size_t          node_capacity_ = 0u;
    std::byte*           node_pool_     = nullptr;
    dlist_policy         list_flags_    = dlist_policy::none;
    dlist_storage_kind   storage_kind_  = dlist_storage_kind::external;
};

} // namespace memkit::detail
