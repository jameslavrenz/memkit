#pragma once

#include "../status.hpp"

namespace memkit::c_api {

[[nodiscard]] status invoke_copy_fn(void* fn, void* dst, const void* src, void* user) noexcept;
void invoke_destroy_fn(void* fn, void* elem, void* user) noexcept;

} // namespace memkit::c_api
