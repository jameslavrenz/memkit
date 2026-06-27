#pragma once

#include "../config.hpp"
#include "../status.hpp"
#include "utility.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "arena.h"

namespace memkit::detail {

enum class storage_kind : unsigned {
    external     = 0u,
    arena_owned  = 1u << 0u,
    heap_owned   = 1u << 1u,
};

struct storage_owner {
    std::byte*  data          = nullptr;
    std::size_t bytes         = 0u;
    unsigned    kind          = 0u;
    ::arena*    arena         = nullptr;

    [[nodiscard]] bool valid() const noexcept { return data != nullptr && bytes > 0u; }

    void release() noexcept
    {
        if ((kind & static_cast<unsigned>(storage_kind::heap_owned)) != 0u && data != nullptr) {
#if MEMKIT_ALLOW_HEAP
            std::free(data);
#endif
        }
        data  = nullptr;
        bytes = 0u;
        kind  = 0u;
        arena = nullptr;
    }

    [[nodiscard]] static storage_owner from_external(std::byte* data, std::size_t bytes) noexcept
    {
        return storage_owner{data, bytes, 0u, nullptr};
    }

    [[nodiscard]] static status from_arena(
        ::arena* arena_ptr,
        std::size_t bytes,
        std::size_t alignment,
        storage_owner& out
    )
    {
        if (arena_ptr == nullptr) {
            return status::null_ptr;
        }

        void* ptr = nullptr;
        const arena_status_t st = arena_alloc(arena_ptr, bytes, alignment, &ptr);
        if (!arena_status_ok(st)) {
            return status::oom;
        }

        out.data  = static_cast<std::byte*>(ptr);
        out.bytes = bytes;
        out.kind  = static_cast<unsigned>(storage_kind::arena_owned);
        out.arena = arena_ptr;
        return status::ok;
    }

    [[nodiscard]] static status from_heap(
        std::size_t bytes,
        storage_owner& out
    )
    {
#if MEMKIT_ALLOW_HEAP
        void* const ptr = std::malloc(bytes);
        if (ptr == nullptr) {
            return status::oom;
        }
        out.data  = static_cast<std::byte*>(ptr);
        out.bytes = bytes;
        out.kind  = static_cast<unsigned>(storage_kind::heap_owned);
        out.arena = nullptr;
        return status::ok;
#else
        (void)bytes;
        (void)out;
        return status::unsupported;
#endif
    }
};

} // namespace memkit::detail
