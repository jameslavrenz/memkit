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

enum class list_policy : std::uint8_t {
    none         = 0u,
    fixed_pool   = 1u << 0u,
    heap_dynamic = 1u << 1u,
};

enum class list_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline list_storage_kind operator|(
    list_storage_kind a,
    list_storage_kind b
) noexcept
{
    return static_cast<list_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

struct list_node_header {
    list_node_header* next = nullptr;
};

template<typename Policy>
class list_core {
public:
    list_core() = default;

    [[nodiscard]] static std::size_t node_stride(std::size_t elem_size) noexcept
    {
        const std::size_t header = align_up(sizeof(list_node_header), alignof(list_node_header));
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
        list_policy list_flags = list_policy::fixed_pool
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
        storage_kind_  = list_storage_kind::external;
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
        list_flags_    = list_policy::heap_dynamic;
        storage_kind_  = list_storage_kind::external;
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
        list_flags_    = list_policy::none;
        storage_kind_  = list_storage_kind::external;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept
    {
        if ((static_cast<unsigned>(list_flags_) &
             static_cast<unsigned>(list_policy::fixed_pool)) != 0u) {
            return node_capacity_;
        }
        return SIZE_MAX;
    }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }
    [[nodiscard]] list_policy flags() const noexcept { return list_flags_; }
    [[nodiscard]] list_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] const Policy& policy() const noexcept { return policy_; }
    [[nodiscard]] std::byte* node_pool() const noexcept { return node_pool_; }
    [[nodiscard]] std::size_t node_stride() const noexcept { return node_stride(policy_.elem_size()); }

    void set_storage_kind(list_storage_kind kind) noexcept { storage_kind_ = kind; }

    [[nodiscard]] bool full() const noexcept
    {
        if ((static_cast<unsigned>(list_flags_) &
             static_cast<unsigned>(list_policy::fixed_pool)) == 0u) {
            return false;
        }
        return size_ >= node_capacity_;
    }

    void clear() noexcept
    {
        while (head_ != nullptr) {
            list_node_header* current = head_;
            unlink_front();
            destroy_node_value(current);
            release_node(current);
        }
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

        list_node_header* current = head_;
        if (out != nullptr) {
            if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
                const status copy_st = policy_.copy_construct(out, node_data(current));
                if (!ok(copy_st)) {
                    return copy_st;
                }
            } else {
                policy_.copy_construct(out, node_data(current));
            }
        } else if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(node_data(current));
        }

        unlink_front();
        release_node(current);
        --size_;
        return status::ok;
    }

    [[nodiscard]] status pop_back(void* out = nullptr) noexcept
    {
        if (empty()) {
            return status::empty;
        }

        list_node_header* prev = nullptr;
        list_node_header* current = node_at(size_ - 1u, &prev);
        if (current == nullptr) {
            return status::empty;
        }

        if (out != nullptr) {
            if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
                const status copy_st = policy_.copy_construct(out, node_data(current));
                if (!ok(copy_st)) {
                    return copy_st;
                }
            } else {
                policy_.copy_construct(out, node_data(current));
            }
        } else if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(node_data(current));
        }

        if (prev == nullptr) {
            head_ = nullptr;
            tail_ = nullptr;
        } else {
            prev->next = nullptr;
            tail_ = prev;
        }

        release_node(current);
        --size_;
        return status::ok;
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
        if (index >= size_) {
            return status::invalid;
        }

        const list_node_header* current = head_;
        for (std::size_t i = 0u; i < index; ++i) {
            current = current->next;
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

        list_node_header* prev = nullptr;
        list_node_header* current = node_at(index, &prev);
        if (current == nullptr || prev == nullptr) {
            return status::invalid;
        }

        list_node_header* new_node = nullptr;
        const status make_st = make_node(value, &new_node);
        if (!ok(make_st)) {
            return make_st;
        }

        new_node->next = current;
        prev->next = new_node;
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

        list_node_header* prev = nullptr;
        list_node_header* current = node_at(index, &prev);
        if (current == nullptr || prev == nullptr) {
            return status::invalid;
        }

        if (out != nullptr) {
            if constexpr (std::is_same_v<Policy, runtime_element_policy>) {
                const status copy_st = policy_.copy_construct(out, node_data(current));
                if (!ok(copy_st)) {
                    return copy_st;
                }
            } else {
                policy_.copy_construct(out, node_data(current));
            }
        } else if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(node_data(current));
        }

        prev->next = current->next;
        release_node(current);
        --size_;
        return status::ok;
    }

    template<typename Predicate>
    [[nodiscard]] status remove_first(Predicate&& pred, void* out = nullptr) noexcept
    {
        std::size_t index = 0u;
        for (list_node_header* current = head_; current != nullptr; current = current->next) {
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

    template<typename Visitor>
    [[nodiscard]] status for_each(Visitor&& visit) const
    {
        std::size_t index = 0u;
        for (const list_node_header* current = head_; current != nullptr; current = current->next) {
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

    [[nodiscard]] status acquire_dynamic_node(list_node_header** out_node) noexcept
    {
        return acquire_node(out_node);
    }

    void release_dynamic_node(list_node_header* node) noexcept
    {
        release_node(node);
    }

private:
    [[nodiscard]] status insert_front(const void* value) noexcept
    {
        list_node_header* new_node = nullptr;
        const status make_st = make_node(value, &new_node);
        if (!ok(make_st)) {
            return make_st;
        }

        new_node->next = head_;
        head_ = new_node;
        if (tail_ == nullptr) {
            tail_ = new_node;
        }
        ++size_;
        return status::ok;
    }

    [[nodiscard]] status insert_back(const void* value) noexcept
    {
        list_node_header* new_node = nullptr;
        const status make_st = make_node(value, &new_node);
        if (!ok(make_st)) {
            return make_st;
        }

        new_node->next = nullptr;
        if (tail_ == nullptr) {
            head_ = new_node;
            tail_ = new_node;
        } else {
            tail_->next = new_node;
            tail_ = new_node;
        }
        ++size_;
        return status::ok;
    }

    [[nodiscard]] list_node_header* pool_node_at(std::size_t index) noexcept
    {
        return reinterpret_cast<list_node_header*>(node_pool_ + index * node_stride());
    }

    [[nodiscard]] status init_free_list() noexcept
    {
        free_list_ = nullptr;
        for (std::size_t i = 0u; i < node_capacity_; ++i) {
            list_node_header* slot = pool_node_at(i);
            slot->next = free_list_;
            free_list_ = slot;
        }
        return status::ok;
    }

    [[nodiscard]] status acquire_node(list_node_header** out_node) noexcept
    {
        if ((static_cast<unsigned>(list_flags_) &
             static_cast<unsigned>(list_policy::fixed_pool)) != 0u) {
            if (free_list_ == nullptr) {
                return status::full;
            }
            list_node_header* slot = free_list_;
            free_list_ = slot->next;
            slot->next = nullptr;
            *out_node = slot;
            return status::ok;
        }

#if MEMKIT_ALLOW_HEAP
        void* memory = std::malloc(node_stride());
        if (memory == nullptr) {
            return status::oom;
        }
        list_node_header* slot = static_cast<list_node_header*>(memory);
        slot->next = nullptr;
        *out_node = slot;
        return status::ok;
#else
        (void)out_node;
        return status::unsupported;
#endif
    }

    void release_node(list_node_header* slot) noexcept
    {
        if (slot == nullptr) {
            return;
        }

        if ((static_cast<unsigned>(list_flags_) &
             static_cast<unsigned>(list_policy::fixed_pool)) != 0u) {
            slot->next = free_list_;
            free_list_ = slot;
            return;
        }

#if MEMKIT_ALLOW_HEAP
        std::free(slot);
#endif
    }

    [[nodiscard]] status make_node(const void* value, list_node_header** out_node) noexcept
    {
        list_node_header* slot = nullptr;
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

    void destroy_node_value(list_node_header* slot) noexcept
    {
        if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(node_data(slot));
        }
    }

    void unlink_front() noexcept
    {
        if (head_ == nullptr) {
            return;
        }
        head_ = head_->next;
        if (head_ == nullptr) {
            tail_ = nullptr;
        }
    }

    [[nodiscard]] list_node_header* node_at(std::size_t index, list_node_header** out_prev) noexcept
    {
        list_node_header* prev = nullptr;
        list_node_header* current = head_;
        for (std::size_t i = 0u; i < index && current != nullptr; ++i) {
            prev = current;
            current = current->next;
        }
        if (out_prev != nullptr) {
            *out_prev = prev;
        }
        return current;
    }

    [[nodiscard]] void* node_data(list_node_header* node) const noexcept
    {
        const std::size_t header = align_up(sizeof(list_node_header), alignof(list_node_header));
        return reinterpret_cast<std::byte*>(node) + header;
    }

    [[nodiscard]] const void* node_data(const list_node_header* node) const noexcept
    {
        return node_data(const_cast<list_node_header*>(node));
    }

    Policy              policy_{};
    list_node_header*   head_          = nullptr;
    list_node_header*   tail_          = nullptr;
    list_node_header*   free_list_     = nullptr;
    std::size_t         size_          = 0u;
    std::size_t         node_capacity_ = 0u;
    std::byte*          node_pool_     = nullptr;
    list_policy         list_flags_    = list_policy::none;
    list_storage_kind   storage_kind_  = list_storage_kind::external;
};

} // namespace memkit::detail
