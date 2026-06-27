#pragma once

#include "../detail/handle_pool_core.hpp"
#include "../status.hpp"
#include "status_cast.hpp"

#include "../../arena.h"
#include "../../handle_pool.h"
#include "../../memkit_config.h"

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>
#include <new>

namespace memkit::c_api {

class handle_pool_box {
public:
    [[nodiscard]] static handle_pool_box& from(handle_pool_t* pool) noexcept
    {
        return *reinterpret_cast<handle_pool_box*>(pool->bytes);
    }

    [[nodiscard]] static const handle_pool_box& from(const handle_pool_t* pool) noexcept
    {
        return *reinterpret_cast<const handle_pool_box*>(pool->bytes);
    }

    [[nodiscard]] static handle_pool_status_t validate_config(
        const handle_pool_config_t* config
    ) noexcept
    {
        if (config == nullptr) {
            return HANDLE_POOL_ERR_NULL;
        }
        if (config->elem_size == 0u || config->capacity == 0u) {
            return HANDLE_POOL_ERR_INVALID;
        }

        const bool has_storage   = config->storage != nullptr;
        const bool has_generations = config->generations != nullptr;
        const bool has_free_stack  = config->free_stack != nullptr;
        const bool wants_owned =
            (config->flags & HANDLE_POOL_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & HANDLE_POOL_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & HANDLE_POOL_FLAG_ARENA_STORAGE) != 0u;

        if (has_storage) {
            const std::size_t required = config->elem_size * config->capacity;
            if (config->storage_bytes < required) {
                return HANDLE_POOL_ERR_INVALID;
            }
        } else if (config->arena == nullptr &&
                   (config->flags & HANDLE_POOL_FLAG_DYNAMIC_STORAGE) == 0u) {
            return HANDLE_POOL_ERR_INVALID;
        }

        if (has_generations &&
            config->generations_bytes <
                ::memkit::detail::handle_pool_core::generations_bytes(config->capacity)) {
            return HANDLE_POOL_ERR_INVALID;
        }

        if (has_free_stack &&
            config->free_stack_bytes <
                ::memkit::detail::handle_pool_core::free_stack_bytes(config->capacity)) {
            return HANDLE_POOL_ERR_INVALID;
        }

        if ((!has_generations || !has_free_stack) && config->arena == nullptr &&
            (config->flags & HANDLE_POOL_FLAG_DYNAMIC_STORAGE) == 0u &&
            (!has_storage || !wants_owned)) {
            return HANDLE_POOL_ERR_INVALID;
        }

        return HANDLE_POOL_OK;
    }

    [[nodiscard]] handle_pool_status_t init(const handle_pool_config_t* config) noexcept
    {
        const handle_pool_status_t valid = validate_config(config);
        if (!handle_pool_status_ok(valid)) {
            return valid;
        }

        c_flags_ = config->flags & ~(
            HANDLE_POOL_FLAG_OWNS_STORAGE |
            HANDLE_POOL_FLAG_OWNS_META |
            HANDLE_POOL_FLAG_DYNAMIC_STORAGE |
            HANDLE_POOL_FLAG_ARENA_STORAGE |
            HANDLE_POOL_FLAG_FIXED_CAPACITY
        );

        std::byte* storage = nullptr;
        const handle_pool_status_t storage_status = allocate_storage(config, &storage);
        if (!handle_pool_status_ok(storage_status)) {
            core_.reset_state();
            return storage_status;
        }

        std::uint16_t* generations = nullptr;
        std::uint32_t* free_stack  = nullptr;
        const handle_pool_status_t meta_status =
            allocate_meta(config, &generations, &free_stack);
        if (!handle_pool_status_ok(meta_status)) {
            release_storage();
            core_.reset_state();
            return meta_status;
        }

        const status st = core_.init(
            storage,
            generations,
            free_stack,
            config->capacity,
            config->elem_size,
            bytes_alignment(config->elem_size)
        );
        if (!ok(st)) {
            release_meta();
            release_storage();
            core_.reset_state();
            return to_handle_pool_status(st);
        }

        if (config->storage != nullptr && (config->flags & HANDLE_POOL_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= HANDLE_POOL_FLAG_OWNS_STORAGE;
        }

        c_flags_ |= HANDLE_POOL_FLAG_FIXED_CAPACITY;
        return HANDLE_POOL_OK;
    }

    void deinit() noexcept
    {
        release_storage();
        release_meta();
        core_.reset_state();
        c_flags_ = 0u;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }

    [[nodiscard]] ::memkit::detail::handle_pool_core& core() noexcept { return core_; }
    [[nodiscard]] const ::memkit::detail::handle_pool_core& core() const noexcept { return core_; }

private:
    [[nodiscard]] static std::size_t bytes_alignment(std::size_t elem_size) noexcept
    {
        if (elem_size == 0u) {
            return alignof(std::max_align_t);
        }
        const std::size_t align = elem_size & (~elem_size + 1u);
        return align > 0u ? align : 1u;
    }

    [[nodiscard]] handle_pool_status_t alloc_raw(
        const handle_pool_config_t* config,
        std::size_t bytes,
        std::size_t alignment,
        void** out_ptr
    ) noexcept
    {
        if (config->arena != nullptr) {
            const arena_status_t status = arena_alloc(config->arena, bytes, alignment, out_ptr);
            if (!arena_status_ok(status)) {
                return status == ARENA_ERR_OOM ? HANDLE_POOL_ERR_OOM : HANDLE_POOL_ERR_INVALID;
            }
            return HANDLE_POOL_OK;
        }

#if MEMKIT_ALLOW_HEAP
        void* const ptr = std::malloc(bytes);
        if (ptr == nullptr) {
            return HANDLE_POOL_ERR_OOM;
        }
        *out_ptr = ptr;
        return HANDLE_POOL_OK;
#else
        (void)bytes;
        (void)alignment;
        (void)out_ptr;
        return HANDLE_POOL_ERR_OOM;
#endif
    }

    [[nodiscard]] handle_pool_status_t allocate_storage(
        const handle_pool_config_t* config,
        std::byte** out_storage
    ) noexcept
    {
        if (config->storage != nullptr) {
            *out_storage = static_cast<std::byte*>(config->storage);
            core_.set_storage_kind(::memkit::detail::handle_pool_storage_kind::external);
            return HANDLE_POOL_OK;
        }

        void* ptr = nullptr;
        const std::size_t bytes = config->elem_size * config->capacity;
        const std::size_t alignment = bytes_alignment(config->elem_size);
        const handle_pool_status_t status = alloc_raw(config, bytes, alignment, &ptr);
        if (!handle_pool_status_ok(status)) {
            return status;
        }

        *out_storage = static_cast<std::byte*>(ptr);
        core_.set_storage_kind(::memkit::detail::handle_pool_storage_kind::owns);

        if (config->arena != nullptr) {
            core_.set_storage_kind(
                ::memkit::detail::handle_pool_storage_kind::owns |
                ::memkit::detail::handle_pool_storage_kind::arena
            );
            c_flags_ |= HANDLE_POOL_FLAG_OWNS_STORAGE | HANDLE_POOL_FLAG_ARENA_STORAGE;
        }
#if MEMKIT_ALLOW_HEAP
        else if ((config->flags & HANDLE_POOL_FLAG_DYNAMIC_STORAGE) != 0u) {
            core_.set_storage_kind(
                ::memkit::detail::handle_pool_storage_kind::owns |
                ::memkit::detail::handle_pool_storage_kind::heap
            );
            c_flags_ |= HANDLE_POOL_FLAG_OWNS_STORAGE | HANDLE_POOL_FLAG_DYNAMIC_STORAGE;
        }
#endif

        return HANDLE_POOL_OK;
    }

    [[nodiscard]] handle_pool_status_t allocate_meta(
        const handle_pool_config_t* config,
        std::uint16_t** out_generations,
        std::uint32_t** out_free_stack
    ) noexcept
    {
        if (config->generations != nullptr && config->free_stack != nullptr) {
            *out_generations = config->generations;
            *out_free_stack  = config->free_stack;
            return HANDLE_POOL_OK;
        }

        void* generations_ptr = nullptr;
        void* free_stack_ptr    = nullptr;

        if (config->generations == nullptr) {
            const handle_pool_status_t status = alloc_raw(
                config,
                ::memkit::detail::handle_pool_core::generations_bytes(config->capacity),
                alignof(std::uint16_t),
                &generations_ptr
            );
            if (!handle_pool_status_ok(status)) {
                return status;
            }
            *out_generations = static_cast<std::uint16_t*>(generations_ptr);
        } else {
            *out_generations = config->generations;
        }

        if (config->free_stack == nullptr) {
            const handle_pool_status_t status = alloc_raw(
                config,
                ::memkit::detail::handle_pool_core::free_stack_bytes(config->capacity),
                alignof(std::uint32_t),
                &free_stack_ptr
            );
            if (!handle_pool_status_ok(status)) {
                if (generations_ptr != nullptr) {
#if MEMKIT_ALLOW_HEAP
                    if (config->arena == nullptr) {
                        std::free(generations_ptr);
                    }
#endif
                    *out_generations = config->generations;
                }
                return status;
            }
            *out_free_stack = static_cast<std::uint32_t*>(free_stack_ptr);
        } else {
            *out_free_stack = config->free_stack;
        }

        if (generations_ptr != nullptr || free_stack_ptr != nullptr) {
            c_flags_ |= HANDLE_POOL_FLAG_OWNS_META;
            if (config->arena != nullptr) {
                c_flags_ |= HANDLE_POOL_FLAG_ARENA_STORAGE;
            }
#if MEMKIT_ALLOW_HEAP
            else if ((config->flags & HANDLE_POOL_FLAG_DYNAMIC_STORAGE) != 0u) {
                c_flags_ |= HANDLE_POOL_FLAG_DYNAMIC_STORAGE;
            }
#endif
        }

        return HANDLE_POOL_OK;
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
        if ((c_flags_ & HANDLE_POOL_FLAG_OWNS_STORAGE) == 0u) {
            return;
        }

        release_owned_block(core_.storage(), HANDLE_POOL_FLAG_DYNAMIC_STORAGE);
    }

    void release_meta() noexcept
    {
        if ((c_flags_ & HANDLE_POOL_FLAG_OWNS_META) == 0u) {
            return;
        }

        release_owned_block(core_.generations(), HANDLE_POOL_FLAG_DYNAMIC_STORAGE);
        release_owned_block(core_.free_stack(), HANDLE_POOL_FLAG_DYNAMIC_STORAGE);
    }

    ::memkit::detail::handle_pool_core core_{};
    unsigned                      c_flags_ = 0u;
};

} // namespace memkit::c_api
