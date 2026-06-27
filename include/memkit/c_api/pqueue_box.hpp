#pragma once

#include "../detail/compare_policy.hpp"
#include "../detail/pqueue_core.hpp"
#include "../status.hpp"
#include "element_callback_bridge.hpp"
#include "status_cast.hpp"

#include "../../arena.h"
#include "../../memkit_config.h"
#include "../../pqueue.h"

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>
#include <cstring>
#include <new>

namespace memkit::c_api {

class pqueue_box {
public:
    [[nodiscard]] static pqueue_box& from(pqueue_t* pqueue) noexcept
    {
        return *reinterpret_cast<pqueue_box*>(pqueue->bytes);
    }

    [[nodiscard]] static const pqueue_box& from(const pqueue_t* pqueue) noexcept
    {
        return *reinterpret_cast<const pqueue_box*>(pqueue->bytes);
    }

    [[nodiscard]] static pqueue_status_t validate_config(const pqueue_config_t* config) noexcept
    {
        if (config == nullptr) {
            return PQUEUE_ERR_NULL;
        }
        if (config->elem_size == 0u) {
            return PQUEUE_ERR_INVALID;
        }

        const bool caller_storage = config->storage != nullptr;
        const bool wants_pqueue_storage =
            (config->flags & PQUEUE_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & PQUEUE_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & PQUEUE_FLAG_ARENA_STORAGE) != 0u;

        if (!caller_storage && config->capacity == 0u && config->arena == nullptr &&
            (config->flags & PQUEUE_FLAG_DYNAMIC_STORAGE) == 0u) {
            return PQUEUE_ERR_INVALID;
        }

        if (!caller_storage && !wants_pqueue_storage && config->arena == nullptr) {
            return PQUEUE_ERR_INVALID;
        }

        if (caller_storage) {
            if (config->capacity == 0u) {
                return PQUEUE_ERR_INVALID;
            }
            const std::size_t required = config->elem_size * config->capacity;
            if (config->storage_bytes < required) {
                return PQUEUE_ERR_INVALID;
            }
        }

        return PQUEUE_OK;
    }

    [[nodiscard]] pqueue_status_t init(const pqueue_config_t* config) noexcept
    {
        const pqueue_status_t valid = validate_config(config);
        if (!pqueue_status_ok(valid)) {
            return valid;
        }

        std::size_t capacity = config->capacity;
        if (config->storage == nullptr && capacity == 0u) {
            capacity = 1u;
        }

        elem_cb_.set(
            config->elem_size,
            reinterpret_cast<void*>(config->copy_fn),
            reinterpret_cast<void*>(config->destroy_fn),
            config->user
        );
        const ::memkit::detail::runtime_element_policy policy = elem_cb_.as_policy();

        ::memkit::detail::runtime_compare_policy compare{
            config->elem_size,
            config->compare_fn,
            config->user,
        };

        c_flags_ = config->flags & ~(
            PQUEUE_FLAG_OWNS_STORAGE |
            PQUEUE_FLAG_DYNAMIC_STORAGE |
            PQUEUE_FLAG_ARENA_STORAGE
        );
        arena_ = config->arena;

        ::memkit::detail::pqueue_policy qp = ::memkit::detail::pqueue_policy::none;
        if ((config->flags & PQUEUE_FLAG_GROWABLE) != 0u) {
            qp = ::memkit::detail::pqueue_policy::growable;
        }

        std::byte* storage = nullptr;
        const pqueue_status_t alloc_status = allocate_storage(config, capacity, &storage);
        if (!pqueue_status_ok(alloc_status)) {
            core_.reset_state();
            return alloc_status;
        }

        const status st = core_.init(policy, compare, storage, capacity, qp);
        if (st != status::ok) {
            release_storage();
            core_.reset_state();
            return to_pqueue_status(st);
        }

        if (config->storage != nullptr && (config->flags & PQUEUE_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= PQUEUE_FLAG_OWNS_STORAGE;
        }

        return PQUEUE_OK;
    }

    void deinit() noexcept
    {
        core_.clear();
        release_storage();
        core_.reset_state();
        c_flags_ = 0u;
        arena_ = nullptr;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }
    [[nodiscard]] arena_t* arena() const noexcept { return arena_; }

    [[nodiscard]] ::memkit::detail::pqueue_core<
        ::memkit::detail::runtime_element_policy,
        ::memkit::detail::runtime_compare_policy>& core() noexcept
    {
        return core_;
    }

    [[nodiscard]] const ::memkit::detail::pqueue_core<
        ::memkit::detail::runtime_element_policy,
        ::memkit::detail::runtime_compare_policy>& core() const noexcept
    {
        return core_;
    }

    [[nodiscard]] pqueue_status_t ensure_capacity(std::size_t min_capacity) noexcept
    {
        if (core_.capacity() >= min_capacity) {
            return PQUEUE_OK;
        }

        if ((c_flags_ & PQUEUE_FLAG_GROWABLE) == 0u) {
            return PQUEUE_ERR_FULL;
        }

        const std::size_t new_capacity = grow_capacity(core_.capacity(), min_capacity);
        if (new_capacity < min_capacity) {
            return PQUEUE_ERR_OOM;
        }

        return reallocate(new_capacity);
    }

    [[nodiscard]] pqueue_status_t reserve(std::size_t min_capacity) noexcept
    {
        if (min_capacity <= core_.capacity()) {
            return PQUEUE_OK;
        }

        if ((c_flags_ & PQUEUE_FLAG_GROWABLE) == 0u) {
            return PQUEUE_ERR_FULL;
        }

        const std::size_t new_capacity = grow_capacity(core_.capacity(), min_capacity);
        return reallocate(new_capacity);
    }

private:
    [[nodiscard]] static std::size_t bytes_alignment(std::size_t elem_size) noexcept
    {
        if (elem_size == 0u) {
            return alignof(std::max_align_t);
        }
        const std::size_t align = elem_size & (~elem_size + 1u);
        return align > 0u ? align : 1u;
    }

    [[nodiscard]] static std::size_t grow_capacity(std::size_t current, std::size_t required) noexcept
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

    [[nodiscard]] pqueue_status_t copy_range(
        std::byte* dst,
        const std::byte* src,
        std::size_t count
    ) noexcept
    {
        const std::size_t elem_size = core_.policy().elem_size();
        for (std::size_t i = 0u; i < count; ++i) {
            const status st = core_.policy().copy_construct(
                dst + (i * elem_size),
                src + (i * elem_size)
            );
            if (!ok(st)) {
                return to_pqueue_status(st);
            }
        }
        return PQUEUE_OK;
    }

    [[nodiscard]] pqueue_status_t allocate_storage(
        const pqueue_config_t* config,
        std::size_t capacity,
        std::byte** out_storage
    ) noexcept
    {
        const std::size_t bytes = config->elem_size * capacity;

        if (config->storage != nullptr) {
            *out_storage = static_cast<std::byte*>(config->storage);
            core_.set_storage_kind(::memkit::detail::pqueue_storage_kind::external);
            return PQUEUE_OK;
        }

        if (config->arena != nullptr) {
            void* ptr = nullptr;
            const std::size_t alignment = bytes_alignment(config->elem_size);
            const arena_status_t status =
                arena_alloc(config->arena, bytes, alignment, &ptr);
            if (!arena_status_ok(status)) {
                return status == ARENA_ERR_OOM ? PQUEUE_ERR_OOM : PQUEUE_ERR_INVALID;
            }

            *out_storage = static_cast<std::byte*>(ptr);
            core_.set_storage_kind(
                ::memkit::detail::pqueue_storage_kind::owns | ::memkit::detail::pqueue_storage_kind::arena
            );
            c_flags_ |= PQUEUE_FLAG_OWNS_STORAGE | PQUEUE_FLAG_ARENA_STORAGE;
            return PQUEUE_OK;
        }

#if MEMKIT_ALLOW_HEAP
        if ((config->flags & PQUEUE_FLAG_DYNAMIC_STORAGE) != 0u) {
            void* const ptr = std::malloc(bytes);
            if (ptr == nullptr) {
                return PQUEUE_ERR_OOM;
            }

            *out_storage = static_cast<std::byte*>(ptr);
            core_.set_storage_kind(
                ::memkit::detail::pqueue_storage_kind::owns | ::memkit::detail::pqueue_storage_kind::heap
            );
            c_flags_ |= PQUEUE_FLAG_OWNS_STORAGE | PQUEUE_FLAG_DYNAMIC_STORAGE;
            return PQUEUE_OK;
        }
#endif

        (void)bytes;
        return PQUEUE_ERR_INVALID;
    }

    [[nodiscard]] pqueue_status_t reallocate(std::size_t new_capacity) noexcept
    {
        const std::size_t elem_size = core_.policy().elem_size();
        const std::size_t bytes = elem_size * new_capacity;
        const std::size_t alignment = bytes_alignment(elem_size);

#if MEMKIT_ALLOW_HEAP
        if ((c_flags_ & PQUEUE_FLAG_DYNAMIC_STORAGE) != 0u) {
            void* const new_storage = std::realloc(core_.storage(), bytes);
            if (new_storage == nullptr) {
                return PQUEUE_ERR_OOM;
            }

            core_.set_storage(static_cast<std::byte*>(new_storage), new_capacity);
            return PQUEUE_OK;
        }
#endif

        if (arena_ != nullptr) {
            void* new_ptr = nullptr;
            const arena_status_t status = arena_alloc(arena_, bytes, alignment, &new_ptr);
            if (!arena_status_ok(status)) {
                return status == ARENA_ERR_OOM ? PQUEUE_ERR_OOM : PQUEUE_ERR_INVALID;
            }

            const pqueue_status_t copy_status = copy_range(
                static_cast<std::byte*>(new_ptr),
                core_.storage(),
                core_.size()
            );
            if (!pqueue_status_ok(copy_status)) {
                return copy_status;
            }

            release_storage();
            core_.set_storage_kind(
                ::memkit::detail::pqueue_storage_kind::owns | ::memkit::detail::pqueue_storage_kind::arena
            );
            c_flags_ |= PQUEUE_FLAG_OWNS_STORAGE | PQUEUE_FLAG_ARENA_STORAGE;
            c_flags_ &= ~static_cast<unsigned>(PQUEUE_FLAG_DYNAMIC_STORAGE);
            core_.set_storage(static_cast<std::byte*>(new_ptr), new_capacity);
            return PQUEUE_OK;
        }

#if MEMKIT_ALLOW_HEAP
        void* const new_storage = std::malloc(bytes);
        if (new_storage == nullptr) {
            return PQUEUE_ERR_OOM;
        }

        const pqueue_status_t copy_status = copy_range(
            static_cast<std::byte*>(new_storage),
            core_.storage(),
            core_.size()
        );
        if (!pqueue_status_ok(copy_status)) {
            std::free(new_storage);
            return copy_status;
        }

        release_storage();
        core_.set_storage_kind(
            ::memkit::detail::pqueue_storage_kind::owns | ::memkit::detail::pqueue_storage_kind::heap
        );
        c_flags_ |= PQUEUE_FLAG_OWNS_STORAGE | PQUEUE_FLAG_DYNAMIC_STORAGE;
        core_.set_storage(static_cast<std::byte*>(new_storage), new_capacity);
        return PQUEUE_OK;
#else
        (void)bytes;
        (void)alignment;
        (void)new_capacity;
        return PQUEUE_ERR_OOM;
#endif
    }

    void release_storage() noexcept
    {
        if ((c_flags_ & PQUEUE_FLAG_OWNS_STORAGE) == 0u || core_.storage() == nullptr) {
            return;
        }

#if MEMKIT_ALLOW_HEAP
        if ((c_flags_ & PQUEUE_FLAG_DYNAMIC_STORAGE) != 0u) {
            std::free(core_.storage());
        }
#endif

        core_.set_storage_kind(::memkit::detail::pqueue_storage_kind::external);
    }

    element_callback_bridge elem_cb_{};
    ::memkit::detail::pqueue_core<
        ::memkit::detail::runtime_element_policy,
        ::memkit::detail::runtime_compare_policy> core_{};
    unsigned c_flags_ = 0u;
    arena_t* arena_   = nullptr;
};

} // namespace memkit::c_api
