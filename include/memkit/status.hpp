#pragma once

#include <cstdint>

namespace memkit {

enum class status : std::uint8_t {
    ok = 0,
    null_ptr,
    invalid,
    empty,
    full,
    oom,
    unsupported,
    not_found,
};

[[nodiscard]] constexpr bool ok(status s) noexcept { return s == status::ok; }

} // namespace memkit
