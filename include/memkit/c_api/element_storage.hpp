#pragma once

#include "../status.hpp"

#include "../../arena.h"
#include "../../memkit_config.h"

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>
#include <cstdint>
#include <new>

namespace memkit::c_api::detail {

struct element_storage_config {
    std::size_t elem_size;
    std::size_t capacity;
    void*       caller_storage;
    std::size_t caller_storage_bytes;
    arena_t*    arena;
    unsigned    flags;
    unsigned    flag_owns_storage;
    unsigned    flag_dynamic_storage;
    unsigned    flag_arena_storage;
};

enum class element_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline element_storage_kind operator|(
    element_storage_kind a,
    element_storage_kind b
) noexcept
{
    return static_cast<element_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

[[nodiscard]] inline std::size_t elem_bytes_alignment(std::size_t elem_size) noexcept
{
    if (elem_size == 0u) {
        return alignof(std::max_align_t);
    }

    const std::size_t align = elem_size & (~elem_size + 1u);
    return align > 0u ? align : 1u;
}

[[nodiscard]] inline status allocate_element_storage(
    const element_storage_config& config,
    std::byte** out,
    element_storage_kind* kind,
    unsigned* inout_c_flags
) noexcept
{
    const std::size_t bytes = config.elem_size * config.capacity;

    if (config.caller_storage != nullptr) {
        *out  = static_cast<std::byte*>(config.caller_storage);
        *kind = element_storage_kind::external;
        return status::ok;
    }

    if (config.arena != nullptr) {
        void* ptr = nullptr;
        const std::size_t alignment = elem_bytes_alignment(config.elem_size);
        const arena_status_t arena_status =
            arena_alloc(config.arena, bytes, alignment, &ptr);
        if (!arena_status_ok(arena_status)) {
            return arena_status == ARENA_ERR_OOM ? status::oom : status::invalid;
        }

        *out  = static_cast<std::byte*>(ptr);
        *kind = element_storage_kind::owns | element_storage_kind::arena;
        *inout_c_flags |= config.flag_owns_storage | config.flag_arena_storage;
        return status::ok;
    }

#if MEMKIT_ALLOW_HEAP
    if ((config.flags & config.flag_dynamic_storage) != 0u) {
        void* const ptr = std::malloc(bytes);
        if (ptr == nullptr) {
            return status::oom;
        }

        *out  = static_cast<std::byte*>(ptr);
        *kind = element_storage_kind::owns | element_storage_kind::heap;
        *inout_c_flags |= config.flag_owns_storage | config.flag_dynamic_storage;
        return status::ok;
    }
#endif

    (void)bytes;
    return status::invalid;
}

inline void release_element_storage(
    std::byte* storage,
    element_storage_kind kind,
    unsigned c_flags,
    unsigned flag_owns,
    unsigned flag_dynamic
) noexcept
{
    if (storage == nullptr) {
        return;
    }

#if MEMKIT_ALLOW_HEAP
    const std::uint8_t kind_bits = static_cast<std::uint8_t>(kind);
    const bool heap_owned =
        (kind_bits & static_cast<std::uint8_t>(element_storage_kind::heap)) != 0u &&
        (kind_bits & static_cast<std::uint8_t>(element_storage_kind::owns)) != 0u;

    if (heap_owned || ((c_flags & flag_dynamic) != 0u && (c_flags & flag_owns) != 0u)) {
        std::free(storage);
        return;
    }
#else
    (void)kind;
    (void)flag_dynamic;
    if ((c_flags & flag_owns) == 0u) {
        return;
    }
#endif
}

} // namespace memkit::c_api::detail
