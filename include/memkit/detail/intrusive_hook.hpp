#pragma once

#include <cstddef>

namespace memkit::detail {

/** Singly-linked intrusive hook (embed as the first link field in your type). */
struct intrusive_forward_hook {
    intrusive_forward_hook* next = nullptr;

    [[nodiscard]] bool is_linked() const noexcept { return next != nullptr; }
};

/** Circular doubly-linked intrusive hook (Linux list_head style). */
struct intrusive_list_hook {
    intrusive_list_hook* next = this;
    intrusive_list_hook* prev = this;

    void init() noexcept
    {
        next = this;
        prev = this;
    }

    [[nodiscard]] bool is_linked() const noexcept { return next != this; }
};

/** Circular doubly-linked intrusive hook for doubly-linked lists. */
using intrusive_dlist_hook = intrusive_list_hook;

template<typename Parent, typename Hook, Hook Parent::* Member>
[[nodiscard]] inline Parent* container_from_hook(Hook* hook) noexcept
{
    return reinterpret_cast<Parent*>(
        reinterpret_cast<char*>(hook) -
        reinterpret_cast<std::size_t>(&(reinterpret_cast<Parent*>(0)->*Member))
    );
}

template<typename Parent, typename Hook, Hook Parent::* Member>
[[nodiscard]] inline const Parent* container_from_hook(const Hook* hook) noexcept
{
    return container_from_hook<Parent, Hook, Member>(const_cast<Hook*>(hook));
}

class intrusive_forward_list_head {
public:
    intrusive_forward_list_head() noexcept = default;

    [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

    [[nodiscard]] intrusive_forward_hook* front() noexcept { return head_; }
    [[nodiscard]] const intrusive_forward_hook* front() const noexcept { return head_; }

    void push_front(intrusive_forward_hook& node) noexcept
    {
        node.next = head_;
        head_     = &node;
    }

    void pop_front() noexcept
    {
        if (head_ != nullptr) {
            head_ = head_->next;
        }
    }

    void erase(intrusive_forward_hook& node, intrusive_forward_hook* prev) noexcept
    {
        if (prev != nullptr) {
            prev->next = node.next;
        } else if (head_ == &node) {
            head_ = node.next;
        }
        node.next = nullptr;
    }

    void clear() noexcept { head_ = nullptr; }

private:
    intrusive_forward_hook* head_ = nullptr;
};

class intrusive_list_head {
public:
    intrusive_list_head() noexcept { sentinel_.init(); }

    [[nodiscard]] bool empty() const noexcept { return !sentinel_.is_linked(); }

    [[nodiscard]] intrusive_list_hook* front() noexcept { return sentinel_.next; }
    [[nodiscard]] const intrusive_list_hook* front() const noexcept { return sentinel_.next; }

    [[nodiscard]] intrusive_list_hook* back() noexcept { return sentinel_.prev; }
    [[nodiscard]] const intrusive_list_hook* back() const noexcept { return sentinel_.prev; }

    void push_front(intrusive_list_hook& node) noexcept
    {
        insert_after(sentinel_, node);
    }

    void push_back(intrusive_list_hook& node) noexcept
    {
        insert_after(*sentinel_.prev, node);
    }

    void erase(intrusive_list_hook& node) noexcept
    {
        if (!node.is_linked()) {
            return;
        }
        node.prev->next = node.next;
        node.next->prev = node.prev;
        node.init();
    }

    void clear() noexcept { sentinel_.init(); }

    static void insert_after(intrusive_list_hook& pos, intrusive_list_hook& node) noexcept
    {
        node.next       = pos.next;
        node.prev       = &pos;
        pos.next->prev  = &node;
        pos.next        = &node;
    }

private:
    intrusive_list_hook sentinel_{};
};

} // namespace memkit::detail
