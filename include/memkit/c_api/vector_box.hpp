#pragma once

#include "../detail/vector_core.hpp"
#include "../status.hpp"
#include "element_callback_bridge.hpp"
#include "element_storage.hpp"

#include "../../arena.h"
#include "../../memkit_config.h"
#include "../../vector.h"

#include <cstddef>
#include <new>

namespace memkit::c_api {

class vector_box {
public:
    [[nodiscard]] static vector_box& from(vector_t* vector) noexcept
    {
        return *reinterpret_cast<vector_box*>(vector->bytes);
    }

    [[nodiscard]] static const vector_box& from(const vector_t* vector) noexcept
    {
        return *reinterpret_cast<const vector_box*>(vector->bytes);
    }

    [[nodiscard]] static vector_status_t validate_config(const vector_config_t* config) noexcept
    {
        if (config == nullptr) {
            return VECTOR_ERR_NULL;
        }
        if (config->elem_size == 0u) {
            return VECTOR_ERR_INVALID;
        }

        const bool caller_storage = config->storage != nullptr;
        const bool wants_vector_storage =
            (config->flags & VECTOR_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & VECTOR_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & VECTOR_FLAG_ARENA_STORAGE) != 0u;

        if (!caller_storage && config->capacity == 0u && config->arena == nullptr) {
            if ((config->flags & VECTOR_FLAG_DYNAMIC_STORAGE) == 0u) {
                return VECTOR_ERR_INVALID;
            }
        }

        if (!caller_storage && !wants_vector_storage && config->arena == nullptr) {
            return VECTOR_ERR_INVALID;
        }

        if (caller_storage) {
            if (config->capacity == 0u) {
                return VECTOR_ERR_INVALID;
            }

            const std::size_t required = config->elem_size * config->capacity;
            if (config->storage_bytes < required) {
                return VECTOR_ERR_INVALID;
            }
        }

        return VECTOR_OK;
    }

    [[nodiscard]] vector_status_t init(const vector_config_t* config) noexcept
    {
        const vector_status_t valid = validate_config(config);
        if (!vector_status_ok(valid)) {
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
            VECTOR_FLAG_OWNS_STORAGE |
            VECTOR_FLAG_DYNAMIC_STORAGE |
            VECTOR_FLAG_ARENA_STORAGE |
            VECTOR_FLAG_GROWABLE
        );

        ::memkit::detail::growable_policy grow_flags = ::memkit::detail::growable_policy::none;
        if ((config->flags & VECTOR_FLAG_GROWABLE) != 0u) {
            grow_flags = ::memkit::detail::growable_policy::growable;
        }

        std::size_t capacity = config->capacity;
        if (config->storage == nullptr && capacity == 0u) {
            capacity = 1u;
        }

        std::byte* storage = nullptr;
        detail::element_storage_kind storage_kind = detail::element_storage_kind::external;
        const detail::element_storage_config storage_config{
            config->elem_size,
            capacity,
            config->storage,
            config->storage_bytes,
            config->arena,
            config->flags,
            VECTOR_FLAG_OWNS_STORAGE,
            VECTOR_FLAG_DYNAMIC_STORAGE,
            VECTOR_FLAG_ARENA_STORAGE,
        };
        const status alloc_status = detail::allocate_element_storage(
            storage_config, &storage, &storage_kind, &c_flags_
        );
        if (alloc_status != status::ok) {
            core_.reset_state();
            return static_cast<vector_status_t>(alloc_status);
        }

        core_.set_storage_kind(
            static_cast<::memkit::detail::linear_storage_kind>(static_cast<std::uint8_t>(storage_kind))
        );

        const status st = core_.init(policy, storage, capacity, grow_flags);
        if (st != status::ok) {
            detail::release_element_storage(
                storage,
                storage_kind,
                c_flags_,
                VECTOR_FLAG_OWNS_STORAGE,
                VECTOR_FLAG_DYNAMIC_STORAGE
            );
            core_.reset_state();
            return static_cast<vector_status_t>(st);
        }

        configure_grow_alloc(config);

        if (config->storage != nullptr && (config->flags & VECTOR_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= VECTOR_FLAG_OWNS_STORAGE;
        }

        return VECTOR_OK;
    }

    void deinit() noexcept
    {
        core_.clear();
        detail::release_element_storage(
            core_.storage(),
            static_cast<detail::element_storage_kind>(
                static_cast<std::uint8_t>(core_.storage_kind())
            ),
            c_flags_,
            VECTOR_FLAG_OWNS_STORAGE,
            VECTOR_FLAG_DYNAMIC_STORAGE
        );
        core_.set_storage_kind(::memkit::detail::linear_storage_kind::external);
        core_.reset_state();
        c_flags_ = 0u;
        arena_   = nullptr;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }

    [[nodiscard]] ::memkit::detail::vector_core<::memkit::detail::runtime_element_policy>& core() noexcept
    {
        return core_;
    }

    [[nodiscard]] const ::memkit::detail::vector_core<::memkit::detail::runtime_element_policy>& core() const noexcept
    {
        return core_;
    }

private:
    [[nodiscard]] static status arena_grow_alloc(
        void* ctx,
        std::size_t size,
        std::size_t align,
        void** out
    ) noexcept
    {
        const arena_status_t st = arena_alloc(static_cast<arena_t*>(ctx), size, align, out);
        if (!arena_status_ok(st)) {
            return st == ARENA_ERR_OOM ? status::oom : status::invalid;
        }
        return status::ok;
    }

    void configure_grow_alloc(const vector_config_t* config) noexcept
    {
        arena_ = config->arena;
        if (arena_ != nullptr) {
            core_.set_grow_alloc({arena_, arena_grow_alloc});
        }
    }

    element_callback_bridge elem_cb_{};
    ::memkit::detail::vector_core<::memkit::detail::runtime_element_policy> core_{};
    unsigned                                            c_flags_ = 0u;
    arena_t*                                            arena_   = nullptr;
};

} // namespace memkit::c_api
