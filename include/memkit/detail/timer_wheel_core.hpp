#pragma once

#include "../status.hpp"
#include "intrusive_hook.hpp"
#include "utility.hpp"

#include <cstddef>
#include <cstdint>

namespace memkit::detail {

struct timer_wheel_node {
    intrusive_list_hook hook{};
    std::uint32_t       deadline   = 0u;
    void (*callback)(void* user) = nullptr;
    void*               user       = nullptr;
};

template<std::size_t NumSlots>
class timer_wheel_core {
public:
    static_assert(NumSlots >= 2u, "Timer wheel requires at least two slots");
    static_assert(is_power_of_two(NumSlots), "Timer wheel slot count must be a power of two");

    timer_wheel_core() = default;

    [[nodiscard]] status init() noexcept
    {
        reset_state();
        return status::ok;
    }

    void reset_state() noexcept
    {
        for (auto& bucket : buckets_) {
            bucket.clear();
        }
        current_slot_ = 0u;
        tick_count_   = 0u;
    }

    [[nodiscard]] std::size_t slot_count() const noexcept { return NumSlots; }
    [[nodiscard]] std::size_t current_slot() const noexcept { return current_slot_; }
    [[nodiscard]] std::uint32_t tick_count() const noexcept { return tick_count_; }

    [[nodiscard]] status schedule(timer_wheel_node& node, std::uint32_t delay_ticks) noexcept
    {
        if (delay_ticks == 0u) {
            return status::invalid;
        }
        if (node.callback == nullptr) {
            return status::invalid;
        }
        if (node.hook.is_linked()) {
            return status::invalid;
        }

        node.deadline = tick_count_ + delay_ticks;
        const std::size_t slot = static_cast<std::size_t>(node.deadline) & (NumSlots - 1u);
        buckets_[slot].push_back(node.hook);
        return status::ok;
    }

    void cancel(timer_wheel_node& node) noexcept
    {
        if (!node.hook.is_linked()) {
            return;
        }
        for (auto& bucket : buckets_) {
            bucket.erase(node.hook);
        }
        node.deadline = 0u;
    }

    [[nodiscard]] status tick(std::size_t steps = 1u) noexcept
    {
        if (steps == 0u) {
            return status::invalid;
        }

        for (std::size_t step = 0u; step < steps; ++step) {
            ++tick_count_;
            current_slot_ = (current_slot_ + 1u) & (NumSlots - 1u);
            dispatch_bucket(current_slot_);
        }

        return status::ok;
    }

private:
    void dispatch_bucket(std::size_t slot) noexcept
    {
        intrusive_list_head pending{};

        while (!buckets_[slot].empty()) {
            intrusive_list_hook& hook = *buckets_[slot].front();
            timer_wheel_node* const node =
                container_from_hook<timer_wheel_node, intrusive_list_hook, &timer_wheel_node::hook>(
                    &hook
                );
            buckets_[slot].erase(hook);

            if (node->deadline <= tick_count_) {
                if (node->callback != nullptr) {
                    node->callback(node->user);
                }
            } else {
                pending.push_back(node->hook);
            }
        }

        while (!pending.empty()) {
            intrusive_list_hook& hook = *pending.front();
            pending.erase(hook);
            buckets_[slot].push_back(hook);
        }
    }

    intrusive_list_head buckets_[NumSlots]{};
    std::size_t         current_slot_ = 0u;
    std::uint32_t       tick_count_   = 0u;
};

} // namespace memkit::detail
