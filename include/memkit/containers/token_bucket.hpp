#pragma once

#include "../status.hpp"

#include <cstddef>
#include <cstdint>
#include <algorithm>

namespace memkit {

/** Tick-based token bucket for rate limiting (UART, CAN, network I/O). */
class TokenBucket {
public:
    TokenBucket() noexcept = default;

    [[nodiscard]] status init(
        std::uint32_t capacity,
        std::uint32_t refill_per_tick
    ) noexcept
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        capacity_        = capacity;
        refill_per_tick_ = refill_per_tick;
        tokens_          = capacity;
        return status::ok;
    }

    [[nodiscard]] std::uint32_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::uint32_t tokens() const noexcept { return tokens_; }
    [[nodiscard]] std::uint32_t refill_per_tick() const noexcept { return refill_per_tick_; }

    void refill(std::uint32_t ticks = 1u) noexcept
    {
        if (ticks == 0u || capacity_ == 0u) {
            return;
        }

        const std::uint64_t added =
            static_cast<std::uint64_t>(refill_per_tick_) * static_cast<std::uint64_t>(ticks);
        const std::uint64_t next =
            static_cast<std::uint64_t>(tokens_) + added;
        tokens_ = static_cast<std::uint32_t>(std::min(next, static_cast<std::uint64_t>(capacity_)));
    }

    [[nodiscard]] status try_consume(std::uint32_t count = 1u) noexcept
    {
        if (count == 0u) {
            return status::invalid;
        }
        if (tokens_ < count) {
            return status::empty;
        }

        tokens_ -= count;
        return status::ok;
    }

    [[nodiscard]] status consume(std::uint32_t count = 1u) noexcept
    {
        return try_consume(count);
    }

    void reset() noexcept { tokens_ = capacity_; }

private:
    std::uint32_t capacity_        = 0u;
    std::uint32_t refill_per_tick_ = 0u;
    std::uint32_t tokens_          = 0u;
};

} // namespace memkit
