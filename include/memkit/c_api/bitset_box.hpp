#pragma once

#include "../detail/bitset_core.hpp"
#include "../status.hpp"
#include "element_storage.hpp"
#include "status_cast.hpp"

#include "../../arena.h"
#include "../../bitset.h"
#include "../../memkit_config.h"

#include <cstddef>
#include <new>

namespace memkit::c_api {

class bitset_box {
public:
    [[nodiscard]] static bitset_box& from(bitset_t* bitset) noexcept
    {
        return *reinterpret_cast<bitset_box*>(bitset->bytes);
    }

    [[nodiscard]] static const bitset_box& from(const bitset_t* bitset) noexcept
    {
        return *reinterpret_cast<const bitset_box*>(bitset->bytes);
    }

    [[nodiscard]] static bitset_status_t validate_config(const bitset_config_t* config) noexcept
    {
        if (config == nullptr) {
            return BITSET_ERR_NULL;
        }
        if (config->capacity == 0u) {
            return BITSET_ERR_INVALID;
        }

        const bool has_storage = config->storage != nullptr;
        const bool wants_owned_storage =
            (config->flags & BITSET_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & BITSET_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & BITSET_FLAG_ARENA_STORAGE) != 0u;

        if (has_storage) {
            const std::size_t required = ::memkit::detail::bitset_core::storage_bytes(config->capacity);
            if (config->storage_bytes < required) {
                return BITSET_ERR_INVALID;
            }
        } else if (config->arena == nullptr &&
                   (config->flags & BITSET_FLAG_DYNAMIC_STORAGE) == 0u) {
            return BITSET_ERR_INVALID;
        }

        if (!has_storage && !wants_owned_storage && config->arena == nullptr) {
            return BITSET_ERR_INVALID;
        }

        return BITSET_OK;
    }

    [[nodiscard]] bitset_status_t init(const bitset_config_t* config) noexcept
    {
        const bitset_status_t valid = validate_config(config);
        if (!bitset_status_ok(valid)) {
            return valid;
        }

        c_flags_ = config->flags & ~(
            BITSET_FLAG_OWNS_STORAGE |
            BITSET_FLAG_DYNAMIC_STORAGE |
            BITSET_FLAG_ARENA_STORAGE |
            BITSET_FLAG_FIXED_CAPACITY
        );

        std::byte* storage = nullptr;
        detail::element_storage_kind storage_kind = detail::element_storage_kind::external;
        const detail::element_storage_config storage_config{
            1u,
            ::memkit::detail::bitset_core::storage_bytes(config->capacity),
            config->storage,
            config->storage_bytes,
            config->arena,
            config->flags,
            BITSET_FLAG_OWNS_STORAGE,
            BITSET_FLAG_DYNAMIC_STORAGE,
            BITSET_FLAG_ARENA_STORAGE,
        };
        const status alloc_status = detail::allocate_element_storage(
            storage_config, &storage, &storage_kind, &c_flags_
        );
        if (alloc_status != status::ok) {
            core_.reset_state();
            return to_bitset_status(alloc_status);
        }

        core_.set_storage_kind(
            static_cast<::memkit::detail::bitset_storage_kind>(static_cast<std::uint8_t>(storage_kind))
        );

        const status st = core_.init(reinterpret_cast<std::uint8_t*>(storage), config->capacity);
        if (st != status::ok) {
            detail::release_element_storage(
                storage,
                storage_kind,
                c_flags_,
                BITSET_FLAG_OWNS_STORAGE,
                BITSET_FLAG_DYNAMIC_STORAGE
            );
            core_.reset_state();
            return to_bitset_status(st);
        }

        if (config->storage != nullptr && (config->flags & BITSET_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= BITSET_FLAG_OWNS_STORAGE;
        }

        c_flags_ |= BITSET_FLAG_FIXED_CAPACITY;
        core_.clear();
        return BITSET_OK;
    }

    void deinit() noexcept
    {
        detail::release_element_storage(
            reinterpret_cast<std::byte*>(core_.storage()),
            static_cast<detail::element_storage_kind>(
                static_cast<std::uint8_t>(core_.storage_kind())
            ),
            c_flags_,
            BITSET_FLAG_OWNS_STORAGE,
            BITSET_FLAG_DYNAMIC_STORAGE
        );
        core_.set_storage_kind(::memkit::detail::bitset_storage_kind::external);
        core_.reset_state();
        c_flags_ = 0u;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }

    [[nodiscard]] ::memkit::detail::bitset_core& core() noexcept { return core_; }
    [[nodiscard]] const ::memkit::detail::bitset_core& core() const noexcept { return core_; }

private:
    ::memkit::detail::bitset_core core_{};
    unsigned          c_flags_ = 0u;
};

} // namespace memkit::c_api
