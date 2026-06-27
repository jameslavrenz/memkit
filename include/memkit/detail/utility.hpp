#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace memkit::detail {

[[nodiscard]] constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept
{
    const std::size_t mask = alignment - 1u;
    return (value + mask) & ~mask;
}

[[nodiscard]] constexpr bool is_power_of_two(std::size_t value) noexcept
{
    return value != 0u && (value & (value - 1u)) == 0u;
}

[[nodiscard]] constexpr std::size_t bytes_alignment(std::size_t elem_size) noexcept
{
    if (elem_size == 0u) {
        return alignof(std::max_align_t);
    }
    const std::size_t align = elem_size & (~elem_size + 1u);
    return align > 0u ? align : 1u;
}

inline void copy_bytes(void* dst, const void* src, std::size_t bytes) noexcept
{
    if (bytes > 0u) {
        std::memcpy(dst, src, bytes);
    }
}

} // namespace memkit::detail
