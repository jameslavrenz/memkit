#pragma once

#include "../detail/ring_core.hpp"
#include "../status.hpp"
#include "element_callback_bridge.hpp"
#include "element_storage.hpp"

#include "../../arena.h"
#include "../../memkit_config.h"
#include "../../ring.h"

#include <cstddef>
#include <new>

namespace memkit::c_api {

class ring_box {
public:
    [[nodiscard]] static ring_box& from(ring_t* ring) noexcept
    {
        return *reinterpret_cast<ring_box*>(ring->bytes);
    }

    [[nodiscard]] static const ring_box& from(const ring_t* ring) noexcept
    {
        return *reinterpret_cast<const ring_box*>(ring->bytes);
    }

    [[nodiscard]] static ring_status_t validate_config(const ring_config_t* config) noexcept
    {
        if (config == nullptr) {
            return RING_ERR_NULL;
        }
        if (config->elem_size == 0u || config->capacity == 0u) {
            return RING_ERR_INVALID;
        }

        const bool caller_storage = config->storage != nullptr;
        const bool wants_ring_storage =
            (config->flags & RING_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & RING_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & RING_FLAG_ARENA_STORAGE) != 0u;

        if (!caller_storage && !wants_ring_storage && config->arena == nullptr) {
            return RING_ERR_INVALID;
        }

        if (caller_storage) {
            const std::size_t required = config->elem_size * config->capacity;
            if (config->storage_bytes < required) {
                return RING_ERR_INVALID;
            }
        }

        return RING_OK;
    }

    [[nodiscard]] ring_status_t init(const ring_config_t* config) noexcept
    {
        const ring_status_t valid = validate_config(config);
        if (!ring_status_ok(valid)) {
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
            RING_FLAG_OWNS_STORAGE |
            RING_FLAG_DYNAMIC_STORAGE |
            RING_FLAG_ARENA_STORAGE
        );

        std::byte* storage = nullptr;
        detail::element_storage_kind storage_kind = detail::element_storage_kind::external;
        const detail::element_storage_config storage_config{
            config->elem_size,
            config->capacity,
            config->storage,
            config->storage_bytes,
            config->arena,
            config->flags,
            RING_FLAG_OWNS_STORAGE,
            RING_FLAG_DYNAMIC_STORAGE,
            RING_FLAG_ARENA_STORAGE,
        };
        const status alloc_status = detail::allocate_element_storage(
            storage_config, &storage, &storage_kind, &c_flags_
        );
        if (alloc_status != status::ok) {
            core_.reset_state();
            return static_cast<ring_status_t>(alloc_status);
        }

        core_.set_storage_kind(
            static_cast<::memkit::detail::ring_storage_kind>(static_cast<std::uint8_t>(storage_kind))
        );

        ::memkit::detail::ring_policy rp = ::memkit::detail::ring_policy::none;
        if ((config->flags & RING_FLAG_OVERWRITE_ON_FULL) != 0u) {
            rp = ::memkit::detail::ring_policy::overwrite_on_full;
        }

        const status st = core_.init(policy, storage, config->capacity, rp);
        if (st != status::ok) {
            detail::release_element_storage(
                storage,
                storage_kind,
                c_flags_,
                RING_FLAG_OWNS_STORAGE,
                RING_FLAG_DYNAMIC_STORAGE
            );
            core_.reset_state();
            return static_cast<ring_status_t>(st);
        }

        if (config->storage != nullptr && (config->flags & RING_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= RING_FLAG_OWNS_STORAGE;
        }

        return RING_OK;
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
            RING_FLAG_OWNS_STORAGE,
            RING_FLAG_DYNAMIC_STORAGE
        );
        core_.set_storage_kind(::memkit::detail::ring_storage_kind::external);
        core_.reset_state();
        c_flags_ = 0u;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }

    [[nodiscard]] ::memkit::detail::ring_core<::memkit::detail::runtime_element_policy>& core() noexcept
    {
        return core_;
    }

    [[nodiscard]] const ::memkit::detail::ring_core<::memkit::detail::runtime_element_policy>& core() const noexcept
    {
        return core_;
    }

private:
    element_callback_bridge elem_cb_{};
    ::memkit::detail::ring_core<::memkit::detail::runtime_element_policy> core_{};
    unsigned                                            c_flags_ = 0u;
};

} // namespace memkit::c_api
