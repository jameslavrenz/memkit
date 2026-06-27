#pragma once

#include "../stl.hpp"

#include <cstddef>

namespace memkit::detail {

[[nodiscard]] constexpr std::size_t byte_capacity_for(
    std::size_t elem_size,
    std::size_t elem_capacity
) noexcept
{
    return elem_size * elem_capacity;
}

[[nodiscard]] constexpr bool byte_span_fits(
    stl::const_byte_span storage,
    std::size_t elem_size,
    std::size_t elem_capacity
) noexcept
{
    return storage.size() >= byte_capacity_for(elem_size, elem_capacity);
}

template<typename T>
[[nodiscard]] constexpr stl::span<T> as_span(T* data, std::size_t count) noexcept
{
    return stl::span<T>{data, count};
}

template<typename T, std::size_t N>
[[nodiscard]] constexpr stl::span<T> as_span(stl::array<T, N>& storage) noexcept
{
    return stl::span<T>{storage.data(), N};
}

template<typename T, std::size_t N>
[[nodiscard]] constexpr stl::span<const T> as_span(const stl::array<T, N>& storage) noexcept
{
    return stl::span<const T>{storage.data(), N};
}

[[nodiscard]] constexpr stl::byte_span bytes(stl::span<std::byte> s) noexcept
{
    return s;
}

template<typename T>
[[nodiscard]] constexpr stl::byte_span object_bytes(stl::span<T> objects) noexcept
{
    return stl::byte_span{
        reinterpret_cast<std::byte*>(objects.data()),
        objects.size_bytes(),
    };
}

} // namespace memkit::detail
