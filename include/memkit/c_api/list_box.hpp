#pragma once

#include "../detail/list_core.hpp"
#include "../status.hpp"
#include "element_callback_bridge.hpp"
#include "status_cast.hpp"

#include "../../arena.h"
#include "../../list.h"
#include "../../memkit_config.h"

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>
#include <new>

namespace memkit::c_api {

class list_box {
public:
    [[nodiscard]] static list_box& from(list_t* list) noexcept
    {
        return *reinterpret_cast<list_box*>(list->bytes);
    }

    [[nodiscard]] static const list_box& from(const list_t* list) noexcept
    {
        return *reinterpret_cast<const list_box*>(list->bytes);
    }

    [[nodiscard]] static list_status_t validate_config(const list_config_t* config) noexcept
    {
        if (config == nullptr) {
            return LIST_ERR_NULL;
        }
        if (config->elem_size == 0u) {
            return LIST_ERR_INVALID;
        }

        const bool has_pool = config->node_pool != nullptr;
        const bool fixed =
            (config->flags & LIST_FLAG_FIXED_CAPACITY) != 0u || has_pool;

        if (has_pool) {
            if (config->node_capacity == 0u) {
                return LIST_ERR_INVALID;
            }

            const std::size_t stride =
                ::memkit::detail::list_core<::memkit::detail::runtime_element_policy>::node_stride(config->elem_size);
            const std::size_t required = config->node_capacity * stride;
            if (config->node_pool_bytes < required) {
                return LIST_ERR_INVALID;
            }
        }

        if (!fixed && config->arena == nullptr &&
            (config->flags & LIST_FLAG_DYNAMIC_STORAGE) == 0u) {
            return LIST_ERR_INVALID;
        }

        return LIST_OK;
    }

    [[nodiscard]] list_status_t init(const list_config_t* config) noexcept
    {
        const list_status_t valid = validate_config(config);
        if (!list_status_ok(valid)) {
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
            LIST_FLAG_OWNS_STORAGE |
            LIST_FLAG_DYNAMIC_STORAGE |
            LIST_FLAG_ARENA_STORAGE |
            LIST_FLAG_FIXED_CAPACITY
        );
        arena_ = config->arena;

        if (config->node_pool != nullptr || (config->flags & LIST_FLAG_FIXED_CAPACITY) != 0u) {
            std::byte* node_pool = nullptr;
            std::size_t node_capacity = config->node_capacity;
            const list_status_t pool_status =
                allocate_node_pool(config, &node_pool, &node_capacity);
            if (!list_status_ok(pool_status)) {
                core_.reset_state();
                return pool_status;
            }

            const status st = core_.init(
                policy,
                node_pool,
                node_capacity,
                ::memkit::detail::list_policy::fixed_pool
            );
            if (st != status::ok) {
                release_node_pool();
                core_.reset_state();
                return to_list_status(st);
            }

            if (config->node_pool != nullptr && (config->flags & LIST_FLAG_OWNS_STORAGE) != 0u) {
                c_flags_ |= LIST_FLAG_OWNS_STORAGE;
            }
        } else {
            const status st = core_.init_dynamic(policy);
            if (st != status::ok) {
                core_.reset_state();
                return to_list_status(st);
            }
        }

        return LIST_OK;
    }

    void deinit() noexcept
    {
        core_.clear();
        release_node_pool();
        core_.reset_state();
        c_flags_ = 0u;
        arena_ = nullptr;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }
    [[nodiscard]] arena_t* arena() const noexcept { return arena_; }

    [[nodiscard]] ::memkit::detail::list_core<::memkit::detail::runtime_element_policy>& core() noexcept
    {
        return core_;
    }

    [[nodiscard]] const ::memkit::detail::list_core<::memkit::detail::runtime_element_policy>& core() const noexcept
    {
        return core_;
    }

private:
    [[nodiscard]] list_status_t allocate_node_pool(
        const list_config_t* config,
        std::byte** out_pool,
        std::size_t* out_capacity
    ) noexcept
    {
        if (config->node_pool != nullptr) {
            *out_pool = static_cast<std::byte*>(config->node_pool);
            *out_capacity = config->node_capacity;
            c_flags_ |= LIST_FLAG_FIXED_CAPACITY;
            return LIST_OK;
        }

        if (config->node_capacity == 0u) {
            return LIST_OK;
        }

        *out_capacity = config->node_capacity;
        const std::size_t stride =
            ::memkit::detail::list_core<::memkit::detail::runtime_element_policy>::node_stride(config->elem_size);
        const std::size_t bytes = (*out_capacity) * stride;

        void* ptr = nullptr;
        if (config->arena != nullptr) {
            const arena_status_t status =
                arena_alloc(config->arena, bytes, alignof(std::max_align_t), &ptr);
            if (!arena_status_ok(status)) {
                return status == ARENA_ERR_OOM ? LIST_ERR_OOM : LIST_ERR_INVALID;
            }

            *out_pool = static_cast<std::byte*>(ptr);
            core_.set_storage_kind(
                ::memkit::detail::list_storage_kind::owns | ::memkit::detail::list_storage_kind::arena
            );
            c_flags_ |= LIST_FLAG_OWNS_STORAGE | LIST_FLAG_ARENA_STORAGE | LIST_FLAG_FIXED_CAPACITY;
            return LIST_OK;
        }

#if MEMKIT_ALLOW_HEAP
        if ((config->flags & LIST_FLAG_DYNAMIC_STORAGE) != 0u) {
            void* const pool = std::malloc(bytes);
            if (pool == nullptr) {
                return LIST_ERR_OOM;
            }

            *out_pool = static_cast<std::byte*>(pool);
            core_.set_storage_kind(
                ::memkit::detail::list_storage_kind::owns | ::memkit::detail::list_storage_kind::heap
            );
            c_flags_ |= LIST_FLAG_OWNS_STORAGE | LIST_FLAG_DYNAMIC_STORAGE | LIST_FLAG_FIXED_CAPACITY;
            return LIST_OK;
        }
#endif

        (void)bytes;
        return LIST_ERR_INVALID;
    }

    void release_node_pool() noexcept
    {
        if ((c_flags_ & LIST_FLAG_OWNS_STORAGE) == 0u || core_.node_pool() == nullptr) {
            return;
        }

#if MEMKIT_ALLOW_HEAP
        if ((c_flags_ & LIST_FLAG_DYNAMIC_STORAGE) != 0u) {
            std::free(core_.node_pool());
        }
#endif

        core_.set_storage_kind(::memkit::detail::list_storage_kind::external);
    }

    element_callback_bridge elem_cb_{};
    ::memkit::detail::list_core<::memkit::detail::runtime_element_policy> core_{};
    unsigned                                          c_flags_ = 0u;
    arena_t*                                          arena_   = nullptr;
};

} // namespace memkit::c_api
