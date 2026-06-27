#include <memkit/c_api/callback_bridge.hpp>
#include <memkit/c_api/element_callback_bridge.hpp>

#include <cstring>

namespace memkit::c_api {

void element_callback_bridge::set(
    std::size_t size,
    void* copy,
    void* destroy,
    void* user_ptr
) noexcept
{
    elem_size      = size;
    copy_opaque    = copy;
    destroy_opaque = destroy;
    user           = user_ptr;
}

status element_callback_bridge::copy_trampoline(void* dst, const void* src, void* ctx) noexcept
{
    auto* bridge = static_cast<element_callback_bridge*>(ctx);
    if (bridge->copy_opaque != nullptr) {
        return invoke_copy_fn(bridge->copy_opaque, dst, src, bridge->user);
    }
    if (bridge->elem_size > 0u) {
        std::memcpy(dst, src, bridge->elem_size);
    }
    return status::ok;
}

void element_callback_bridge::destroy_trampoline(void* elem, void* ctx) noexcept
{
    auto* bridge = static_cast<element_callback_bridge*>(ctx);
    if (bridge->destroy_opaque != nullptr) {
        invoke_destroy_fn(bridge->destroy_opaque, elem, bridge->user);
    }
}

::memkit::detail::runtime_element_policy element_callback_bridge::as_policy() const noexcept
{
    return ::memkit::detail::runtime_element_policy{
        elem_size,
        copy_trampoline,
        destroy_opaque != nullptr ? destroy_trampoline : nullptr,
        const_cast<element_callback_bridge*>(this),
    };
}

} // namespace memkit::c_api
