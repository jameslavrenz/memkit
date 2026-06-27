#pragma once

#include "../../arena.h"
#include "../../memkit_config.h"

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

namespace memkit::c_api::detail {

template<typename ObjectT>
[[nodiscard]] inline bool allocate_object(arena_t* arena, ObjectT** out_object) noexcept
{
#if MEMKIT_ALLOW_HEAP
    if (arena == nullptr) {
        *out_object = static_cast<ObjectT*>(std::malloc(sizeof(ObjectT)));
        return *out_object != nullptr;
    }
#else
    if (arena == nullptr) {
        *out_object = nullptr;
        return false;
    }
#endif

    void* storage = nullptr;
    const arena_status_t status =
        arena_alloc(arena, sizeof(ObjectT), alignof(ObjectT), &storage);
    if (!arena_status_ok(status)) {
        *out_object = nullptr;
        return false;
    }

    *out_object = static_cast<ObjectT*>(storage);
    return true;
}

template<typename ObjectT>
inline void release_uninitialized_object(arena_t* arena, ObjectT* object) noexcept
{
#if MEMKIT_ALLOW_HEAP
    if (arena == nullptr && object != nullptr) {
        std::free(object);
    }
#else
    (void)arena;
    (void)object;
#endif
}

} // namespace memkit::c_api::detail
