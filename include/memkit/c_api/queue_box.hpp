#pragma once

#include "../detail/queue_core.hpp"
#include "../status.hpp"
#include "element_callback_bridge.hpp"
#include "element_storage.hpp"

#include "../../arena.h"
#include "../../memkit_config.h"
#include "../../queue.h"

#include <cstddef>
#include <new>

namespace memkit::c_api {

class queue_box {
public:
    [[nodiscard]] static queue_box& from(queue_t* queue) noexcept
    {
        return *reinterpret_cast<queue_box*>(queue->bytes);
    }

    [[nodiscard]] static const queue_box& from(const queue_t* queue) noexcept
    {
        return *reinterpret_cast<const queue_box*>(queue->bytes);
    }

    [[nodiscard]] static queue_status_t validate_config(const queue_config_t* config) noexcept
    {
        if (config == nullptr) {
            return QUEUE_ERR_NULL;
        }
        if (config->elem_size == 0u) {
            return QUEUE_ERR_INVALID;
        }

        const bool caller_storage = config->storage != nullptr;
        const bool wants_queue_storage =
            (config->flags & QUEUE_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & QUEUE_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & QUEUE_FLAG_ARENA_STORAGE) != 0u;

        if (!caller_storage && config->capacity == 0u && config->arena == nullptr &&
            (config->flags & QUEUE_FLAG_DYNAMIC_STORAGE) == 0u) {
            return QUEUE_ERR_INVALID;
        }

        if (!caller_storage && !wants_queue_storage && config->arena == nullptr) {
            return QUEUE_ERR_INVALID;
        }

        if (caller_storage) {
            if (config->capacity == 0u) {
                return QUEUE_ERR_INVALID;
            }

            const std::size_t required = config->elem_size * config->capacity;
            if (config->storage_bytes < required) {
                return QUEUE_ERR_INVALID;
            }
        }

        return QUEUE_OK;
    }

    [[nodiscard]] queue_status_t init(const queue_config_t* config) noexcept
    {
        const queue_status_t valid = validate_config(config);
        if (!queue_status_ok(valid)) {
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
            QUEUE_FLAG_OWNS_STORAGE |
            QUEUE_FLAG_DYNAMIC_STORAGE |
            QUEUE_FLAG_ARENA_STORAGE |
            QUEUE_FLAG_GROWABLE
        );

        ::memkit::detail::ring_buffer_policy grow_flags = ::memkit::detail::ring_buffer_policy::none;
        if ((config->flags & QUEUE_FLAG_GROWABLE) != 0u) {
            grow_flags = ::memkit::detail::ring_buffer_policy::growable;
        }

        std::size_t capacity = config->capacity;
        if (capacity == 0u) {
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
            QUEUE_FLAG_OWNS_STORAGE,
            QUEUE_FLAG_DYNAMIC_STORAGE,
            QUEUE_FLAG_ARENA_STORAGE,
        };
        const status alloc_status = detail::allocate_element_storage(
            storage_config, &storage, &storage_kind, &c_flags_
        );
        if (alloc_status != status::ok) {
            core_.reset_state();
            return static_cast<queue_status_t>(alloc_status);
        }

        core_.set_storage_kind(
            static_cast<::memkit::detail::ring_buffer_storage_kind>(static_cast<std::uint8_t>(storage_kind))
        );

        const status st = core_.init(policy, storage, capacity, grow_flags);
        if (st != status::ok) {
            detail::release_element_storage(
                storage,
                storage_kind,
                c_flags_,
                QUEUE_FLAG_OWNS_STORAGE,
                QUEUE_FLAG_DYNAMIC_STORAGE
            );
            core_.reset_state();
            return static_cast<queue_status_t>(st);
        }

        configure_grow_alloc(config);

        if (config->storage != nullptr && (config->flags & QUEUE_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= QUEUE_FLAG_OWNS_STORAGE;
        }

        return QUEUE_OK;
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
            QUEUE_FLAG_OWNS_STORAGE,
            QUEUE_FLAG_DYNAMIC_STORAGE
        );
        core_.set_storage_kind(::memkit::detail::ring_buffer_storage_kind::external);
        core_.reset_state();
        c_flags_ = 0u;
        arena_   = nullptr;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }

    [[nodiscard]] ::memkit::detail::queue_core<::memkit::detail::runtime_element_policy>& core() noexcept
    {
        return core_;
    }

    [[nodiscard]] const ::memkit::detail::queue_core<::memkit::detail::runtime_element_policy>& core() const noexcept
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

    void configure_grow_alloc(const queue_config_t* config) noexcept
    {
        arena_ = config->arena;
        if (arena_ != nullptr) {
            core_.set_grow_alloc({arena_, arena_grow_alloc});
        }
    }

    element_callback_bridge elem_cb_{};
    ::memkit::detail::queue_core<::memkit::detail::runtime_element_policy> core_{};
    unsigned                                         c_flags_ = 0u;
    arena_t*                                         arena_   = nullptr;
};

} // namespace memkit::c_api
