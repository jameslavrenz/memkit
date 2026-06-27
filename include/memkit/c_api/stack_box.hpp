#pragma once

#include "../detail/vector_core.hpp"
#include "../status.hpp"
#include "element_callback_bridge.hpp"
#include "element_storage.hpp"

#include "../../arena.h"
#include "../../memkit_config.h"
#include "../../stack.h"

#include <cstddef>
#include <new>

namespace memkit::c_api {

class stack_box {
public:
    [[nodiscard]] static stack_box& from(cstack_t* stack) noexcept
    {
        return *reinterpret_cast<stack_box*>(stack->bytes);
    }

    [[nodiscard]] static const stack_box& from(const cstack_t* stack) noexcept
    {
        return *reinterpret_cast<const stack_box*>(stack->bytes);
    }

    [[nodiscard]] static stack_status_t validate_config(const stack_config_t* config) noexcept
    {
        if (config == nullptr) {
            return STACK_ERR_NULL;
        }
        if (config->elem_size == 0u) {
            return STACK_ERR_INVALID;
        }

        const bool caller_storage = config->storage != nullptr;
        const bool wants_stack_storage =
            (config->flags & STACK_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & STACK_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & STACK_FLAG_ARENA_STORAGE) != 0u;

        if (!caller_storage && config->capacity == 0u && config->arena == nullptr &&
            (config->flags & STACK_FLAG_DYNAMIC_STORAGE) == 0u) {
            return STACK_ERR_INVALID;
        }

        if (!caller_storage && !wants_stack_storage && config->arena == nullptr) {
            return STACK_ERR_INVALID;
        }

        if (caller_storage) {
            if (config->capacity == 0u) {
                return STACK_ERR_INVALID;
            }

            const std::size_t required = config->elem_size * config->capacity;
            if (config->storage_bytes < required) {
                return STACK_ERR_INVALID;
            }
        }

        return STACK_OK;
    }

    [[nodiscard]] stack_status_t init(const stack_config_t* config) noexcept
    {
        const stack_status_t valid = validate_config(config);
        if (!stack_status_ok(valid)) {
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
            STACK_FLAG_OWNS_STORAGE |
            STACK_FLAG_DYNAMIC_STORAGE |
            STACK_FLAG_ARENA_STORAGE |
            STACK_FLAG_GROWABLE
        );

        ::memkit::detail::growable_policy grow_flags = ::memkit::detail::growable_policy::none;
        if ((config->flags & STACK_FLAG_GROWABLE) != 0u) {
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
            STACK_FLAG_OWNS_STORAGE,
            STACK_FLAG_DYNAMIC_STORAGE,
            STACK_FLAG_ARENA_STORAGE,
        };
        const status alloc_status = detail::allocate_element_storage(
            storage_config, &storage, &storage_kind, &c_flags_
        );
        if (alloc_status != status::ok) {
            core_.reset_state();
            return static_cast<stack_status_t>(alloc_status);
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
                STACK_FLAG_OWNS_STORAGE,
                STACK_FLAG_DYNAMIC_STORAGE
            );
            core_.reset_state();
            return static_cast<stack_status_t>(st);
        }

        configure_grow_alloc(config);

        if (config->storage != nullptr && (config->flags & STACK_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= STACK_FLAG_OWNS_STORAGE;
        }

        return STACK_OK;
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
            STACK_FLAG_OWNS_STORAGE,
            STACK_FLAG_DYNAMIC_STORAGE
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

    [[nodiscard]] bool full() const noexcept
    {
        if (::memkit::detail::has(
                core_.grow_flags(),
                ::memkit::detail::growable_policy::growable)) {
            return false;
        }
        return core_.size() >= core_.capacity();
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

    void configure_grow_alloc(const stack_config_t* config) noexcept
    {
        arena_ = config->arena;
        if (arena_ != nullptr) {
            core_.set_grow_alloc({arena_, arena_grow_alloc});
        }
    }

    element_callback_bridge elem_cb_{};
    ::memkit::detail::vector_core<::memkit::detail::runtime_element_policy> core_{};
    unsigned                                                             c_flags_ = 0u;
    arena_t*                                                             arena_   = nullptr;
};

} // namespace memkit::c_api
