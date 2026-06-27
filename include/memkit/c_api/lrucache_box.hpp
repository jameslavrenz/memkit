#pragma once

#include "../detail/lrucache_map_core.hpp"
#include "../status.hpp"
#include "element_callback_bridge.hpp"
#include "status_cast.hpp"

#include "../../arena.h"
#include "../../lrucache.h"
#include "../../memkit_config.h"

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>
#include <cstring>
#include <new>

namespace memkit::c_api {

class lrucache_box {
public:
    [[nodiscard]] static lrucache_box& from(lrucache_t* cache) noexcept
    {
        return *reinterpret_cast<lrucache_box*>(cache->bytes);
    }

    [[nodiscard]] static const lrucache_box& from(const lrucache_t* cache) noexcept
    {
        return *reinterpret_cast<const lrucache_box*>(cache->bytes);
    }

    [[nodiscard]] static lrucache_status_t validate_config(const lrucache_config_t* config) noexcept
    {
        if (config == nullptr) {
            return LRUCACHE_ERR_NULL;
        }
        if (config->key_size == 0u || config->capacity == 0u) {
            return LRUCACHE_ERR_INVALID;
        }

        const bool has_entry_pool = config->entry_pool != nullptr;
        const bool has_buckets    = config->buckets != nullptr;
        const bool wants_owned_storage =
            (config->flags & LRUCACHE_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & LRUCACHE_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & LRUCACHE_FLAG_ARENA_STORAGE) != 0u;

        if (has_entry_pool) {
            const std::size_t required =
                ::memkit::detail::lrucache_map_core<
                    ::memkit::detail::runtime_element_policy,
                    ::memkit::detail::runtime_element_policy>::entry_pool_bytes(
                    config->capacity,
                    config->key_size,
                    config->value_size
                );
            if (config->entry_pool_bytes < required) {
                return LRUCACHE_ERR_INVALID;
            }
        } else if (config->arena == nullptr &&
                   (config->flags & LRUCACHE_FLAG_DYNAMIC_STORAGE) == 0u) {
            return LRUCACHE_ERR_INVALID;
        }

        const std::size_t bucket_count = config->bucket_count != 0u
            ? config->bucket_count
            : ::memkit::detail::lrucache_map_core<
                  ::memkit::detail::runtime_element_policy,
                  ::memkit::detail::runtime_element_policy>::default_bucket_count(config->capacity);

        if (has_buckets &&
            config->buckets_bytes <
                ::memkit::detail::lrucache_map_core<
                    ::memkit::detail::runtime_element_policy,
                    ::memkit::detail::runtime_element_policy>::buckets_bytes(bucket_count)) {
            return LRUCACHE_ERR_INVALID;
        }

        if (!has_buckets && config->arena == nullptr &&
            (config->flags & LRUCACHE_FLAG_DYNAMIC_STORAGE) == 0u &&
            (!has_entry_pool || !wants_owned_storage)) {
            return LRUCACHE_ERR_INVALID;
        }

        return LRUCACHE_OK;
    }

    [[nodiscard]] lrucache_status_t init(const lrucache_config_t* config) noexcept
    {
        const lrucache_status_t valid = validate_config(config);
        if (!lrucache_status_ok(valid)) {
            return valid;
        }

        const std::size_t bucket_count = config->bucket_count != 0u
            ? config->bucket_count
            : ::memkit::detail::lrucache_map_core<
                  ::memkit::detail::runtime_element_policy,
                  ::memkit::detail::runtime_element_policy>::default_bucket_count(config->capacity);

        key_cb_.set(
            config->key_size,
            reinterpret_cast<void*>(config->copy_key_fn),
            reinterpret_cast<void*>(config->destroy_key_fn),
            config->user
        );

        value_cb_.set(
            config->value_size,
            reinterpret_cast<void*>(config->copy_value_fn),
            reinterpret_cast<void*>(config->destroy_value_fn),
            config->user
        );

        const ::memkit::detail::runtime_element_policy key_policy = key_cb_.as_policy();
        const ::memkit::detail::runtime_element_policy value_policy = value_cb_.as_policy();

        ::memkit::detail::runtime_hash_key_policy hash_eq_policy{
            config->key_size,
            config->hash_fn,
            config->key_eq_fn,
            config->user,
        };

        c_flags_ = config->flags & ~(
            LRUCACHE_FLAG_OWNS_STORAGE |
            LRUCACHE_FLAG_DYNAMIC_STORAGE |
            LRUCACHE_FLAG_ARENA_STORAGE |
            LRUCACHE_FLAG_FIXED_CAPACITY
        );
        arena_ = config->arena;

        std::byte* entry_pool = nullptr;
        const lrucache_status_t pool_status = allocate_entry_pool(config, &entry_pool);
        if (!lrucache_status_ok(pool_status)) {
            core_.reset_state();
            return pool_status;
        }

        ::memkit::detail::lrucache_entry_header** buckets = nullptr;
        const lrucache_status_t bucket_status = allocate_buckets(config, bucket_count, &buckets);
        if (!lrucache_status_ok(bucket_status)) {
            release_storage();
            core_.reset_state();
            return bucket_status;
        }

        if ((config->entry_pool == nullptr || config->buckets == nullptr) ||
            (config->flags & LRUCACHE_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= LRUCACHE_FLAG_OWNS_STORAGE;
        }

        c_flags_ |= LRUCACHE_FLAG_FIXED_CAPACITY;

        const status st = core_.init(
            key_policy,
            value_policy,
            hash_eq_policy,
            entry_pool,
            config->capacity,
            buckets,
            bucket_count
        );
        if (st != status::ok) {
            release_storage();
            core_.reset_state();
            return to_lrucache_status(st);
        }

        if (config->buckets == nullptr) {
            core_.set_owns_buckets(true);
        }

        return LRUCACHE_OK;
    }

    void deinit() noexcept
    {
        core_.clear();
        release_storage();
        core_.reset_state();
        c_flags_ = 0u;
        arena_   = nullptr;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }
    [[nodiscard]] arena_t* arena() const noexcept { return arena_; }

    [[nodiscard]] ::memkit::detail::lrucache_map_core<
        ::memkit::detail::runtime_element_policy,
        ::memkit::detail::runtime_element_policy>& core() noexcept
    {
        return core_;
    }

    [[nodiscard]] const ::memkit::detail::lrucache_map_core<
        ::memkit::detail::runtime_element_policy,
        ::memkit::detail::runtime_element_policy>& core() const noexcept
    {
        return core_;
    }

private:
    [[nodiscard]] lrucache_status_t alloc_raw(
        const lrucache_config_t* config,
        std::size_t bytes,
        std::size_t alignment,
        void** out_ptr
    ) noexcept
    {
        if (config->arena != nullptr) {
            const arena_status_t status = arena_alloc(config->arena, bytes, alignment, out_ptr);
            if (!arena_status_ok(status)) {
                return status == ARENA_ERR_OOM ? LRUCACHE_ERR_OOM : LRUCACHE_ERR_INVALID;
            }
            return LRUCACHE_OK;
        }

#if MEMKIT_ALLOW_HEAP
        void* const ptr = std::malloc(bytes);
        if (ptr == nullptr) {
            return LRUCACHE_ERR_OOM;
        }
        *out_ptr = ptr;
        return LRUCACHE_OK;
#else
        (void)bytes;
        (void)alignment;
        (void)out_ptr;
        return LRUCACHE_ERR_OOM;
#endif
    }

    [[nodiscard]] lrucache_status_t allocate_entry_pool(
        const lrucache_config_t* config,
        std::byte** out_pool
    ) noexcept
    {
        if (config->entry_pool != nullptr) {
            *out_pool = static_cast<std::byte*>(config->entry_pool);
            return LRUCACHE_OK;
        }

        void* ptr = nullptr;
        const std::size_t bytes =
            ::memkit::detail::lrucache_map_core<
                ::memkit::detail::runtime_element_policy,
                ::memkit::detail::runtime_element_policy>::entry_pool_bytes(
                config->capacity,
                config->key_size,
                config->value_size
            );
        const lrucache_status_t status =
            alloc_raw(config, bytes, alignof(std::max_align_t), &ptr);
        if (!lrucache_status_ok(status)) {
            return status;
        }

        *out_pool = static_cast<std::byte*>(ptr);
        if (config->arena != nullptr) {
            core_.set_storage_kind(
                ::memkit::detail::lrucache_storage_kind::owns |
                ::memkit::detail::lrucache_storage_kind::arena
            );
            c_flags_ |= LRUCACHE_FLAG_ARENA_STORAGE;
        }
#if MEMKIT_ALLOW_HEAP
        else if ((config->flags & LRUCACHE_FLAG_DYNAMIC_STORAGE) != 0u) {
            core_.set_storage_kind(
                ::memkit::detail::lrucache_storage_kind::owns |
                ::memkit::detail::lrucache_storage_kind::heap
            );
            c_flags_ |= LRUCACHE_FLAG_DYNAMIC_STORAGE;
        }
#endif
        return LRUCACHE_OK;
    }

    [[nodiscard]] lrucache_status_t allocate_buckets(
        const lrucache_config_t* config,
        std::size_t bucket_count,
        ::memkit::detail::lrucache_entry_header*** out_buckets
    ) noexcept
    {
        if (config->buckets != nullptr) {
            *out_buckets = reinterpret_cast<::memkit::detail::lrucache_entry_header**>(config->buckets);
            return LRUCACHE_OK;
        }

        void* ptr = nullptr;
        const std::size_t bytes =
            ::memkit::detail::lrucache_map_core<
                ::memkit::detail::runtime_element_policy,
                ::memkit::detail::runtime_element_policy>::buckets_bytes(bucket_count);
        const lrucache_status_t status =
            alloc_raw(config, bytes, alignof(::memkit::detail::lrucache_entry_header*), &ptr);
        if (!lrucache_status_ok(status)) {
            return status;
        }

        *out_buckets = static_cast<::memkit::detail::lrucache_entry_header**>(ptr);
        if (config->arena != nullptr) {
            core_.set_storage_kind(
                core_.storage_kind() | ::memkit::detail::lrucache_storage_kind::owns |
                ::memkit::detail::lrucache_storage_kind::arena
            );
            c_flags_ |= LRUCACHE_FLAG_ARENA_STORAGE;
        }
#if MEMKIT_ALLOW_HEAP
        else if ((config->flags & LRUCACHE_FLAG_DYNAMIC_STORAGE) != 0u) {
            core_.set_storage_kind(
                core_.storage_kind() | ::memkit::detail::lrucache_storage_kind::owns |
                ::memkit::detail::lrucache_storage_kind::heap
            );
            c_flags_ |= LRUCACHE_FLAG_DYNAMIC_STORAGE;
        }
#endif
        return LRUCACHE_OK;
    }

    void release_owned_block(void* ptr) noexcept
    {
        if (ptr == nullptr) {
            return;
        }

#if MEMKIT_ALLOW_HEAP
        if ((c_flags_ & LRUCACHE_FLAG_DYNAMIC_STORAGE) != 0u) {
            std::free(ptr);
        }
#endif
    }

    void release_storage() noexcept
    {
        if ((c_flags_ & LRUCACHE_FLAG_OWNS_STORAGE) == 0u) {
            return;
        }

        release_owned_block(core_.entry_pool());
        if (core_.owns_buckets()) {
            release_owned_block(core_.buckets());
        }

        core_.set_storage_kind(::memkit::detail::lrucache_storage_kind::external);
        core_.set_owns_buckets(false);
    }

    element_callback_bridge key_cb_{};
    element_callback_bridge value_cb_{};
    ::memkit::detail::lrucache_map_core<
        ::memkit::detail::runtime_element_policy,
        ::memkit::detail::runtime_element_policy> core_{};
    unsigned c_flags_ = 0u;
    arena_t* arena_     = nullptr;
};

} // namespace memkit::c_api
