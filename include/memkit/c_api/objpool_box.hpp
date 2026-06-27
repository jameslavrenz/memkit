#pragma once

#include "../detail/objpool_core.hpp"
#include "../status.hpp"
#include "element_callback_bridge.hpp"
#include "status_cast.hpp"

#include "../../arena.h"
#include "../../memkit_config.h"
#include "../../objpool.h"

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>
#include <new>

namespace memkit::c_api {

class objpool_box {
public:
    [[nodiscard]] static objpool_box& from(objpool_t* pool) noexcept
    {
        return *reinterpret_cast<objpool_box*>(pool->bytes);
    }

    [[nodiscard]] static const objpool_box& from(const objpool_t* pool) noexcept
    {
        return *reinterpret_cast<const objpool_box*>(pool->bytes);
    }

    [[nodiscard]] static objpool_status_t validate_config(const objpool_config_t* config) noexcept
    {
        if (config == nullptr) {
            return OBJPOOL_ERR_NULL;
        }
        if (config->elem_size == 0u || config->capacity == 0u) {
            return OBJPOOL_ERR_INVALID;
        }

        const bool has_storage = config->storage != nullptr;
        const bool has_free_stack = config->free_stack != nullptr;
        const bool has_used_bits = config->used_bits != nullptr;
        const bool wants_owned_storage =
            (config->flags & OBJPOOL_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & OBJPOOL_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & OBJPOOL_FLAG_ARENA_STORAGE) != 0u;

        if (has_storage) {
            const std::size_t required =
                config->elem_size * config->capacity;
            if (config->storage_bytes < required) {
                return OBJPOOL_ERR_INVALID;
            }
        } else if (config->arena == nullptr &&
                   (config->flags & OBJPOOL_FLAG_DYNAMIC_STORAGE) == 0u) {
            return OBJPOOL_ERR_INVALID;
        }

        if (has_free_stack &&
            config->free_stack_bytes < ::memkit::detail::objpool_core<::memkit::detail::runtime_element_policy>::free_stack_bytes(config->capacity)) {
            return OBJPOOL_ERR_INVALID;
        }

        if (has_used_bits &&
            config->used_bits_bytes < ::memkit::detail::objpool_core<::memkit::detail::runtime_element_policy>::used_bits_bytes(config->capacity)) {
            return OBJPOOL_ERR_INVALID;
        }

        if (!has_free_stack || !has_used_bits) {
            if (config->arena == nullptr &&
                (config->flags & OBJPOOL_FLAG_DYNAMIC_STORAGE) == 0u &&
                (!has_storage || !wants_owned_storage)) {
                if (!has_free_stack || !has_used_bits) {
                    return OBJPOOL_ERR_INVALID;
                }
            }
        }

        return OBJPOOL_OK;
    }

    [[nodiscard]] objpool_status_t init(const objpool_config_t* config) noexcept
    {
        const objpool_status_t valid = validate_config(config);
        if (!objpool_status_ok(valid)) {
            return valid;
        }

        elem_cb_.set(
            config->elem_size,
            reinterpret_cast<void*>(config->copy_fn),
            reinterpret_cast<void*>(config->destroy_fn),
            config->user
        );
        const ::memkit::detail::runtime_element_policy policy = elem_cb_.as_policy();

        c_flags_ = config->flags & ~(
            OBJPOOL_FLAG_OWNS_STORAGE |
            OBJPOOL_FLAG_OWNS_META |
            OBJPOOL_FLAG_DYNAMIC_STORAGE |
            OBJPOOL_FLAG_ARENA_STORAGE |
            OBJPOOL_FLAG_FIXED_CAPACITY
        );

        std::byte* storage = nullptr;
        const objpool_status_t storage_status = allocate_storage(config, &storage);
        if (!objpool_status_ok(storage_status)) {
            core_.reset_state();
            return storage_status;
        }

        std::uint32_t* free_stack = nullptr;
        std::byte* used_bits = nullptr;
        const objpool_status_t meta_status =
            allocate_meta(config, &free_stack, &used_bits);
        if (!objpool_status_ok(meta_status)) {
            release_storage();
            core_.reset_state();
            return meta_status;
        }

        const status st = core_.init(policy, storage, free_stack, used_bits, config->capacity);
        if (st != status::ok) {
            release_meta();
            release_storage();
            core_.reset_state();
            return to_objpool_status(st);
        }

        if (config->storage != nullptr && (config->flags & OBJPOOL_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= OBJPOOL_FLAG_OWNS_STORAGE;
        }

        c_flags_ |= OBJPOOL_FLAG_FIXED_CAPACITY;
        return OBJPOOL_OK;
    }

    void deinit() noexcept
    {
        core_.clear();
        release_storage();
        release_meta();
        core_.reset_state();
        c_flags_ = 0u;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }

    [[nodiscard]] ::memkit::detail::objpool_core<::memkit::detail::runtime_element_policy>& core() noexcept
    {
        return core_;
    }

    [[nodiscard]] const ::memkit::detail::objpool_core<::memkit::detail::runtime_element_policy>& core() const noexcept
    {
        return core_;
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

    [[nodiscard]] objpool_status_t alloc_raw(
        const objpool_config_t* config,
        std::size_t bytes,
        std::size_t alignment,
        void** out_ptr
    ) noexcept
    {
        if (config->arena != nullptr) {
            const arena_status_t status = arena_alloc(config->arena, bytes, alignment, out_ptr);
            if (!arena_status_ok(status)) {
                return status == ARENA_ERR_OOM ? OBJPOOL_ERR_OOM : OBJPOOL_ERR_INVALID;
            }
            return OBJPOOL_OK;
        }

#if MEMKIT_ALLOW_HEAP
        void* const ptr = std::malloc(bytes);
        if (ptr == nullptr) {
            return OBJPOOL_ERR_OOM;
        }
        *out_ptr = ptr;
        return OBJPOOL_OK;
#else
        (void)bytes;
        (void)alignment;
        (void)out_ptr;
        return OBJPOOL_ERR_OOM;
#endif
    }

    [[nodiscard]] objpool_status_t allocate_storage(
        const objpool_config_t* config,
        std::byte** out_storage
    ) noexcept
    {
        if (config->storage != nullptr) {
            *out_storage = static_cast<std::byte*>(config->storage);
            core_.set_storage_kind(::memkit::detail::objpool_storage_kind::external);
            return OBJPOOL_OK;
        }

        void* ptr = nullptr;
        const std::size_t bytes = config->elem_size * config->capacity;
        const std::size_t alignment = bytes_alignment(config->elem_size);
        const objpool_status_t status = alloc_raw(config, bytes, alignment, &ptr);
        if (!objpool_status_ok(status)) {
            return status;
        }

        *out_storage = static_cast<std::byte*>(ptr);
        core_.set_storage_kind(::memkit::detail::objpool_storage_kind::owns);
        c_flags_ |= OBJPOOL_FLAG_OWNS_STORAGE;

        if (config->arena != nullptr) {
            core_.set_storage_kind(
                ::memkit::detail::objpool_storage_kind::owns | ::memkit::detail::objpool_storage_kind::arena
            );
            c_flags_ |= OBJPOOL_FLAG_ARENA_STORAGE;
        }
#if MEMKIT_ALLOW_HEAP
        else if ((config->flags & OBJPOOL_FLAG_DYNAMIC_STORAGE) != 0u) {
            core_.set_storage_kind(
                ::memkit::detail::objpool_storage_kind::owns | ::memkit::detail::objpool_storage_kind::heap
            );
            c_flags_ |= OBJPOOL_FLAG_DYNAMIC_STORAGE;
        }
#endif

        return OBJPOOL_OK;
    }

    [[nodiscard]] objpool_status_t allocate_meta(
        const objpool_config_t* config,
        std::uint32_t** out_free_stack,
        std::byte** out_used_bits
    ) noexcept
    {
        if (config->free_stack != nullptr && config->used_bits != nullptr) {
            *out_free_stack = config->free_stack;
            *out_used_bits = reinterpret_cast<std::byte*>(config->used_bits);
            return OBJPOOL_OK;
        }

        void* free_ptr = nullptr;
        void* bits_ptr = nullptr;
        const std::size_t free_bytes =
            ::memkit::detail::objpool_core<::memkit::detail::runtime_element_policy>::free_stack_bytes(config->capacity);
        const std::size_t bits_bytes =
            ::memkit::detail::objpool_core<::memkit::detail::runtime_element_policy>::used_bits_bytes(config->capacity);

        if (config->free_stack == nullptr) {
            const objpool_status_t status =
                alloc_raw(config, free_bytes, alignof(std::uint32_t), &free_ptr);
            if (!objpool_status_ok(status)) {
                return status;
            }
            *out_free_stack = static_cast<std::uint32_t*>(free_ptr);
        } else {
            *out_free_stack = config->free_stack;
        }

        if (config->used_bits == nullptr) {
            const objpool_status_t status =
                alloc_raw(config, bits_bytes, alignof(std::uint8_t), &bits_ptr);
            if (!objpool_status_ok(status)) {
                if (free_ptr != nullptr) {
#if MEMKIT_ALLOW_HEAP
                    if (config->arena == nullptr) {
                        std::free(free_ptr);
                    }
#endif
                    *out_free_stack = config->free_stack;
                }
                return status;
            }
            *out_used_bits = static_cast<std::byte*>(bits_ptr);
        } else {
            *out_used_bits = reinterpret_cast<std::byte*>(config->used_bits);
        }

        if (free_ptr != nullptr || bits_ptr != nullptr) {
            c_flags_ |= OBJPOOL_FLAG_OWNS_META;
            if (config->arena != nullptr) {
                c_flags_ |= OBJPOOL_FLAG_ARENA_STORAGE;
            }
#if MEMKIT_ALLOW_HEAP
            else if ((config->flags & OBJPOOL_FLAG_DYNAMIC_STORAGE) != 0u) {
                c_flags_ |= OBJPOOL_FLAG_DYNAMIC_STORAGE;
            }
#endif
        }

        return OBJPOOL_OK;
    }

    void release_owned_block(void* ptr, unsigned dynamic_flag) noexcept
    {
        if (ptr == nullptr) {
            return;
        }

#if MEMKIT_ALLOW_HEAP
        if ((c_flags_ & dynamic_flag) != 0u) {
            std::free(ptr);
        }
#else
        (void)ptr;
        (void)dynamic_flag;
#endif
    }

    void release_storage() noexcept
    {
        if ((c_flags_ & OBJPOOL_FLAG_OWNS_STORAGE) == 0u) {
            return;
        }
        release_owned_block(core_.storage(), OBJPOOL_FLAG_DYNAMIC_STORAGE);
    }

    void release_meta() noexcept
    {
        if ((c_flags_ & OBJPOOL_FLAG_OWNS_META) == 0u) {
            return;
        }
        release_owned_block(core_.free_stack(), OBJPOOL_FLAG_DYNAMIC_STORAGE);
        release_owned_block(core_.used_bits(), OBJPOOL_FLAG_DYNAMIC_STORAGE);
    }

    element_callback_bridge elem_cb_{};
    ::memkit::detail::objpool_core<::memkit::detail::runtime_element_policy> core_{};
    unsigned                                             c_flags_ = 0u;
};

} // namespace memkit::c_api
