#pragma once

#include "config.hpp"
#include "memory/memory.hpp"
#include "status.hpp"

#include <cstddef>
#include <type_traits>

namespace memkit {

template<typename T>
concept TriviallyStorable = std::is_trivially_copyable_v<T>;

template<typename Backing>
concept ArenaBacking = requires(Backing& b, std::size_t n, std::size_t a) {
    { b.data() } -> std::same_as<std::byte*>;
    { b.size() } -> std::convertible_to<std::size_t>;
    { b.allocate(n, a) } -> std::same_as<status>;
};

} // namespace memkit
