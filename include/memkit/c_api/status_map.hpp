#pragma once

#include "../status.hpp"

namespace memkit::c_api {

[[nodiscard]] constexpr int to_c(status s) noexcept
{
    return static_cast<int>(s);
}

[[nodiscard]] constexpr status from_c(int code) noexcept
{
    return static_cast<status>(code);
}

} // namespace memkit::c_api
