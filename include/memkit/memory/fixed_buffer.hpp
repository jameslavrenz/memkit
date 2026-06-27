#pragma once

#include "../detail/storage_view.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <cstdint>

namespace memkit::memory {

/*
 * Non-owning view over caller-provided storage (MCU/MPU).
 * Zero-cost: no allocations, no virtual dispatch.
 */
class fixed_buffer {
public:
    constexpr fixed_buffer() noexcept = default;

    constexpr fixed_buffer(std::byte* data, std::size_t bytes) noexcept
        : base_{data}
        , bytes_{bytes}
    {}

    constexpr fixed_buffer(stl::byte_span view) noexcept
        : base_{view.data()}
        , bytes_{view.size()}
    {}

    template<std::size_t N>
    constexpr fixed_buffer(stl::array<std::byte, N>& storage) noexcept
        : base_{storage.data()}
        , bytes_{N}
    {}

    template<std::size_t N>
    constexpr fixed_buffer(const stl::array<std::byte, N>& storage) noexcept
        : base_{const_cast<std::byte*>(storage.data())}
        , bytes_{N}
    {}

    [[nodiscard]] constexpr std::byte*       data()       noexcept { return base_; }
    [[nodiscard]] constexpr const std::byte* data() const noexcept { return base_; }
    [[nodiscard]] constexpr std::size_t        size() const noexcept { return bytes_; }
    [[nodiscard]] constexpr bool             empty() const noexcept { return bytes_ == 0u; }

    [[nodiscard]] constexpr stl::byte_span       span()       noexcept { return {base_, bytes_}; }
    [[nodiscard]] constexpr stl::const_byte_span span() const noexcept { return {base_, bytes_}; }

private:
    std::byte*  base_  = nullptr;
    std::size_t bytes_ = 0u;
};

} // namespace memkit::memory
