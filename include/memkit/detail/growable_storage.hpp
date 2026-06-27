#pragma once

#include "../config.hpp"
#include "../status.hpp"
#include "element_ops.hpp"

#include <cstddef>
#include <cstdlib>
#include <new>
#include <type_traits>

namespace memkit::detail {

struct grow_alloc {
    void* ctx = nullptr;
    status (*allocate)(void* ctx, std::size_t size, std::size_t align, void** out) = nullptr;
};

[[nodiscard]] inline std::size_t grow_capacity(
    std::size_t current,
    std::size_t required
) noexcept
{
    std::size_t new_capacity = current > 0u ? current : 1u;

    while (new_capacity < required) {
        if (new_capacity > SIZE_MAX / 2u) {
            return required;
        }
        new_capacity *= 2u;
    }

    return new_capacity;
}

class arena_allocator {
public:
    template<typename Arena>
    void bind(Arena& arena) noexcept
    {
        state_       = &arena;
        allocate_fn_ = +[](void* state, std::size_t size, std::size_t align, void** out) -> status {
            return static_cast<Arena*>(state)->allocate(size, align, out);
        };
    }

    void reset() noexcept
    {
        state_       = nullptr;
        allocate_fn_ = nullptr;
    }

    [[nodiscard]] bool bound() const noexcept { return allocate_fn_ != nullptr; }

    [[nodiscard]] status allocate(std::size_t size, std::size_t alignment, void** out_ptr) const
    {
        if (allocate_fn_ == nullptr || state_ == nullptr) {
            return status::invalid;
        }
        return allocate_fn_(state_, size, alignment, out_ptr);
    }

private:
    void* state_ = nullptr;
    status (*allocate_fn_)(void*, std::size_t, std::size_t, void**) = nullptr;
};

enum class storage_source : unsigned {
    external = 0u,
    arena    = 1u,
    heap     = 2u,
};

template<typename T>
inline void move_range(T* dst, T* src, std::size_t count) noexcept
{
    using ops = typed_element_ops<T>;

    for (std::size_t i = 0u; i < count; ++i) {
        ops::construct(&dst[i], std::move(src[i]));
        ops::destroy(&src[i]);
    }
}

template<typename T>
[[nodiscard]] inline status linear_reallocate(
    std::byte*& storage,
    std::size_t& capacity,
    std::size_t count,
    std::size_t new_capacity,
    storage_source& source,
    arena_allocator* arena
)
{
    if (new_capacity < count) {
        return status::invalid;
    }

    void* new_ptr = nullptr;
    const std::size_t bytes = new_capacity * sizeof(T);

    if (source == storage_source::heap) {
#if MEMKIT_ALLOW_HEAP
        if (storage != nullptr) {
            new_ptr = std::realloc(storage, bytes);
        } else {
            new_ptr = std::malloc(bytes);
        }
        if (new_ptr == nullptr) {
            return status::oom;
        }
#else
        (void)bytes;
        return status::unsupported;
#endif
    } else if (source == storage_source::arena) {
        if (arena == nullptr || !arena->bound()) {
            return status::invalid;
        }
        const status st = arena->allocate(bytes, alignof(T), &new_ptr);
        if (!ok(st)) {
            return st;
        }
    } else {
#if MEMKIT_ALLOW_HEAP
        if (arena != nullptr && arena->bound()) {
            const status st = arena->allocate(bytes, alignof(T), &new_ptr);
            if (!ok(st)) {
                return st;
            }
            source = storage_source::arena;
        } else {
            new_ptr = std::malloc(bytes);
            if (new_ptr == nullptr) {
                return status::oom;
            }
            source = storage_source::heap;
        }
#else
        if (arena == nullptr || !arena->bound()) {
            return status::unsupported;
        }
        const status st = arena->allocate(bytes, alignof(T), &new_ptr);
        if (!ok(st)) {
            return st;
        }
        source = storage_source::arena;
#endif
    }

    T* const old_data = reinterpret_cast<T*>(storage);
    T* const new_data = reinterpret_cast<T*>(new_ptr);

    if (count > 0u && old_data != nullptr) {
        move_range(new_data, old_data, count);
    }

#if MEMKIT_ALLOW_HEAP
    if (source == storage_source::heap && storage != nullptr && new_ptr != storage) {
        std::free(storage);
    }
#endif

    storage  = static_cast<std::byte*>(new_ptr);
    capacity = new_capacity;
    return status::ok;
}

template<typename T>
[[nodiscard]] inline T* ptr_at(std::byte* storage, std::size_t index) noexcept
{
    return reinterpret_cast<T*>(storage + index * sizeof(T));
}

template<typename T>
[[nodiscard]] inline const T* ptr_at(const std::byte* storage, std::size_t index) noexcept
{
    return reinterpret_cast<const T*>(storage + index * sizeof(T));
}

template<typename T>
[[nodiscard]] inline status ring_relinearize(
    std::byte*& storage,
    std::size_t& capacity,
    std::size_t& head,
    std::size_t& tail,
    std::size_t count,
    std::size_t new_capacity,
    storage_source& source,
    arena_allocator* arena
)
{
    if (new_capacity < count) {
        return status::invalid;
    }

    void* new_ptr = nullptr;
    const std::size_t bytes = new_capacity * sizeof(T);

    if (source == storage_source::heap) {
#if MEMKIT_ALLOW_HEAP
        new_ptr = std::malloc(bytes);
        if (new_ptr == nullptr) {
            return status::oom;
        }
#else
        (void)bytes;
        return status::unsupported;
#endif
    } else if (source == storage_source::arena) {
        if (arena == nullptr || !arena->bound()) {
            return status::invalid;
        }
        const status st = arena->allocate(bytes, alignof(T), &new_ptr);
        if (!ok(st)) {
            return st;
        }
    } else {
#if MEMKIT_ALLOW_HEAP
        if (arena != nullptr && arena->bound()) {
            const status st = arena->allocate(bytes, alignof(T), &new_ptr);
            if (!ok(st)) {
                return st;
            }
            source = storage_source::arena;
        } else {
            new_ptr = std::malloc(bytes);
            if (new_ptr == nullptr) {
                return status::oom;
            }
            source = storage_source::heap;
        }
#else
        if (arena == nullptr || !arena->bound()) {
            return status::unsupported;
        }
        const status st = arena->allocate(bytes, alignof(T), &new_ptr);
        if (!ok(st)) {
            return st;
        }
        source = storage_source::arena;
#endif
    }

    using ops = typed_element_ops<T>;
    T* const new_data = reinterpret_cast<T*>(new_ptr);

    for (std::size_t i = 0u; i < count; ++i) {
        T* const src = ptr_at<T>(storage, (tail + i) % capacity);
        ops::construct(&new_data[i], std::move(*src));
        ops::destroy(src);
    }

#if MEMKIT_ALLOW_HEAP
    if (source == storage_source::heap && storage != nullptr) {
        std::free(storage);
    }
#endif

    storage  = static_cast<std::byte*>(new_ptr);
    capacity = new_capacity;
    head     = count;
    tail     = 0u;
    return status::ok;
}

} // namespace memkit::detail
