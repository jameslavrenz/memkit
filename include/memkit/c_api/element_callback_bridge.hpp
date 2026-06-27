#pragma once

#include "../detail/element_policy.hpp"

#include <cstddef>

namespace memkit::c_api {

struct element_callback_bridge {
    void*       copy_opaque    = nullptr;
    void*       destroy_opaque = nullptr;
    void*       user           = nullptr;
    std::size_t elem_size      = 0u;

    void set(std::size_t size, void* copy, void* destroy, void* user_ptr) noexcept;

    [[nodiscard]] static status copy_trampoline(void* dst, const void* src, void* ctx) noexcept;
    static void destroy_trampoline(void* elem, void* ctx) noexcept;

    [[nodiscard]] ::memkit::detail::runtime_element_policy as_policy() const noexcept;
};

} // namespace memkit::c_api
