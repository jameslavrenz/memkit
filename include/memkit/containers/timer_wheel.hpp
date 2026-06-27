#pragma once

#include "../detail/timer_wheel_core.hpp"
#include "../status.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace memkit {

using TimerWheelNode = detail::timer_wheel_node;

/** Hashed timing wheel for deferred callbacks (intrusive nodes, no heap). */
template<std::size_t NumSlots>
class TimerWheel {
public:
    static_assert(NumSlots >= 2u, "Timer wheel requires at least two slots");
    static_assert(detail::is_power_of_two(NumSlots), "Timer wheel slot count must be a power of two");

    TimerWheel() noexcept = default;

    TimerWheel(TimerWheel&& other) noexcept
        : core_{std::move(other.core_)}
    {
        other.core_.reset_state();
    }

    TimerWheel& operator=(TimerWheel&& other) noexcept
    {
        if (this != &other) {
            core_ = std::move(other.core_);
            other.core_.reset_state();
        }
        return *this;
    }

    TimerWheel(const TimerWheel&)            = delete;
    TimerWheel& operator=(const TimerWheel&) = delete;

    [[nodiscard]] status init() noexcept { return core_.init(); }

    [[nodiscard]] std::size_t slot_count() const noexcept { return core_.slot_count(); }
    [[nodiscard]] std::size_t current_slot() const noexcept { return core_.current_slot(); }
    [[nodiscard]] std::uint32_t tick_count() const noexcept { return core_.tick_count(); }

    [[nodiscard]] status schedule(TimerWheelNode& node, std::uint32_t delay_ticks) noexcept
    {
        return core_.schedule(node, delay_ticks);
    }

    void cancel(TimerWheelNode& node) noexcept { core_.cancel(node); }

    [[nodiscard]] status tick(std::size_t steps = 1u) noexcept { return core_.tick(steps); }

private:
    detail::timer_wheel_core<NumSlots> core_{};
};

} // namespace memkit
