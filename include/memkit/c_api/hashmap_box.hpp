#pragma once

#include "../../hashmap.h"
#include "../../memkit_config.h"
#include "../../memkit_object_sizes.h"
#include "../detail/hashmap_map_core.hpp"
#include "element_callback_bridge.hpp"
#include "status_cast.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

namespace memkit::c_api {

class hashmap_box {
public:
    [[nodiscard]] static hashmap_box& from(hashmap_t* map) noexcept
    {
        return *reinterpret_cast<hashmap_box*>(map->bytes);
    }

    [[nodiscard]] static const hashmap_box& from(const hashmap_t* map) noexcept
    {
        return *reinterpret_cast<const hashmap_box*>(map->bytes);
    }

    [[nodiscard]] static hashmap_status_t validate_config(const hashmap_config_t* config) noexcept
    {
        if (config == nullptr) {
            return HASHMAP_ERR_NULL;
        }
        if (config->key_size == 0u) {
            return HASHMAP_ERR_INVALID;
        }
        if (config->strategy != HASHMAP_STRATEGY_CHAINING &&
            config->strategy != HASHMAP_STRATEGY_OPEN_ADDRESSING) {
            return HASHMAP_ERR_INVALID;
        }

        const bool caller_storage = config->storage != nullptr;
        const bool wants_map_storage =
            (config->flags & HASHMAP_FLAG_OWNS_STORAGE) != 0u ||
            (config->flags & HASHMAP_FLAG_DYNAMIC_STORAGE) != 0u ||
            (config->flags & HASHMAP_FLAG_ARENA_STORAGE) != 0u;

        if (!caller_storage && config->bucket_count == 0u && config->arena == nullptr) {
#if !MEMKIT_ALLOW_HEAP
            if ((config->flags & HASHMAP_FLAG_DYNAMIC_STORAGE) == 0u) {
                return HASHMAP_ERR_INVALID;
            }
#else
            if ((config->flags & HASHMAP_FLAG_DYNAMIC_STORAGE) == 0u) {
                return HASHMAP_ERR_INVALID;
            }
#endif
        }

        if (!caller_storage && !wants_map_storage && config->arena == nullptr) {
            return HASHMAP_ERR_INVALID;
        }

        if (caller_storage) {
            if (config->bucket_count == 0u) {
                return HASHMAP_ERR_INVALID;
            }

            if (config->strategy == HASHMAP_STRATEGY_CHAINING) {
                const std::size_t required = config->bucket_count * sizeof(hashmap_node_t*);
                if (config->storage_bytes < required) {
                    return HASHMAP_ERR_INVALID;
                }
            }
        }

        return HASHMAP_OK;
    }

    [[nodiscard]] hashmap_status_t init(const hashmap_config_t* config) noexcept
    {
        const hashmap_status_t valid = validate_config(config);
        if (!hashmap_status_ok(valid)) {
            return valid;
        }

        c_flags_ = config->flags & ~(
            HASHMAP_FLAG_OWNS_STORAGE |
            HASHMAP_FLAG_DYNAMIC_STORAGE |
            HASHMAP_FLAG_ARENA_STORAGE
        );
        arena_ = config->arena;

        std::size_t bucket_count = config->bucket_count;
        if (bucket_count == 0u) {
            bucket_count = default_buckets;
        }

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
        ::memkit::detail::runtime_hash_key_policy hash_policy{
            config->key_size,
            config->hash_fn,
            config->key_eq_fn,
            config->user,
        };

        const auto map_policy =
            (config->flags & HASHMAP_FLAG_GROWABLE) != 0u
                ? ::memkit::detail::hashmap_policy::growable
                : ::memkit::detail::hashmap_policy::none;

        std::size_t key_offset   = 0u;
        std::size_t value_offset = 0u;
        std::size_t slot_stride  = 0u;

        if (config->strategy == HASHMAP_STRATEGY_OPEN_ADDRESSING) {
            slot_stride = ::memkit::detail::hashmap_map_core::open_slot_stride(
                config->key_size,
                config->value_size
            );
            const std::size_t key_alignment = ::memkit::detail::bytes_alignment(config->key_size);
            key_offset   = ::memkit::detail::align_up(1u, key_alignment);
            value_offset = key_offset + config->key_size;

            if (config->storage != nullptr) {
                const std::size_t required = bucket_count * slot_stride;
                if (config->storage_bytes < required) {
                    return HASHMAP_ERR_INVALID;
                }
            }
        }

        const hashmap_status_t storage_status = allocate_bucket_storage(
            config,
            bucket_count,
            key_offset,
            value_offset,
            slot_stride,
            key_policy,
            value_policy,
            hash_policy,
            map_policy
        );
        if (!hashmap_status_ok(storage_status)) {
            core_.reset_state();
            return storage_status;
        }

        if (config->storage != nullptr && (config->flags & HASHMAP_FLAG_OWNS_STORAGE) != 0u) {
            c_flags_ |= HASHMAP_FLAG_OWNS_STORAGE;
        }

        if (arena_ != nullptr) {
            core_.bind_allocator(arena_, &hashmap_box::arena_alloc_trampoline);
        }

        return HASHMAP_OK;
    }

    void deinit() noexcept
    {
        core_.clear();
        release_bucket_storage();
        core_.reset_state();
        c_flags_ = 0u;
        arena_   = nullptr;
    }

    [[nodiscard]] unsigned c_flags() const noexcept { return c_flags_; }
    void set_c_flags(unsigned flags) noexcept { c_flags_ = flags; }
    [[nodiscard]] arena_t* arena() const noexcept { return arena_; }

    [[nodiscard]] ::memkit::detail::hashmap_map_core& core() noexcept { return core_; }
    [[nodiscard]] const ::memkit::detail::hashmap_map_core& core() const noexcept { return core_; }

private:
    element_callback_bridge key_cb_{};
    element_callback_bridge value_cb_{};

    static constexpr std::size_t default_buckets = 8u;

    static ::memkit::status arena_alloc_trampoline(
        void* ctx,
        std::size_t bytes,
        std::size_t alignment,
        void** out_ptr
    ) noexcept
    {
        const arena_status_t status =
            arena_alloc(static_cast<arena_t*>(ctx), bytes, alignment, out_ptr);
        if (!arena_status_ok(status)) {
            return status == ARENA_ERR_OOM ? ::memkit::status::oom : ::memkit::status::invalid;
        }
        return ::memkit::status::ok;
    }

    [[nodiscard]] hashmap_status_t allocate_bucket_storage(
        const hashmap_config_t* config,
        std::size_t bucket_count,
        std::size_t key_offset,
        std::size_t value_offset,
        std::size_t slot_stride,
        const ::memkit::detail::runtime_element_policy& key_policy,
        const ::memkit::detail::runtime_element_policy& value_policy,
        const ::memkit::detail::runtime_hash_key_policy& hash_policy,
        ::memkit::detail::hashmap_policy map_policy
    ) noexcept
    {
        if (config->storage != nullptr) {
            if (config->strategy == HASHMAP_STRATEGY_CHAINING) {
                auto* buckets = static_cast<::memkit::detail::hashmap_chain_node**>(config->storage);
                std::memset(buckets, 0, bucket_count * sizeof(hashmap_node_t*));
                const status st = core_.init_chaining(
                    buckets,
                    bucket_count,
                    key_policy,
                    value_policy,
                    hash_policy,
                    map_policy
                );
                return to_hashmap_status(st);
            }

            auto* slots = static_cast<std::byte*>(config->storage);
            const status st = core_.init_open(
                slots,
                bucket_count,
                key_offset,
                value_offset,
                slot_stride,
                key_policy,
                value_policy,
                hash_policy,
                map_policy
            );
            return to_hashmap_status(st);
        }

        if (config->strategy == HASHMAP_STRATEGY_CHAINING) {
            void* ptr = nullptr;
            const std::size_t bytes = bucket_count * sizeof(hashmap_node_t*);
            const hashmap_status_t alloc_status =
                alloc_raw(bytes, alignof(hashmap_node_t*), &ptr);
            if (!hashmap_status_ok(alloc_status)) {
                return alloc_status;
            }

            auto* buckets = static_cast<::memkit::detail::hashmap_chain_node**>(ptr);
            std::memset(buckets, 0, bytes);

            const status st = core_.init_chaining(
                buckets,
                bucket_count,
                key_policy,
                value_policy,
                hash_policy,
                map_policy
            );
            if (!ok(st)) {
                return to_hashmap_status(st);
            }

            c_flags_ |= HASHMAP_FLAG_OWNS_STORAGE;
            if (arena_ != nullptr) {
                c_flags_ |= HASHMAP_FLAG_ARENA_STORAGE;
                core_.set_storage_kind(
                    ::memkit::detail::hashmap_storage_kind::owns |
                    ::memkit::detail::hashmap_storage_kind::arena
                );
            }
#if MEMKIT_ALLOW_HEAP
            else {
                c_flags_ |= HASHMAP_FLAG_DYNAMIC_STORAGE;
                core_.set_storage_kind(
                    ::memkit::detail::hashmap_storage_kind::owns |
                    ::memkit::detail::hashmap_storage_kind::heap
                );
            }
#endif
            return HASHMAP_OK;
        }

        void* ptr = nullptr;
        const std::size_t bytes = bucket_count * slot_stride;
        const hashmap_status_t alloc_status = alloc_raw(bytes, key_offset, &ptr);
        if (!hashmap_status_ok(alloc_status)) {
            return alloc_status;
        }

        auto* slots = static_cast<std::byte*>(ptr);
        const status st = core_.init_open(
            slots,
            bucket_count,
            key_offset,
            value_offset,
            slot_stride,
            key_policy,
            value_policy,
            hash_policy,
            map_policy
        );
        if (!ok(st)) {
            return to_hashmap_status(st);
        }

        c_flags_ |= HASHMAP_FLAG_OWNS_STORAGE;
        if (arena_ != nullptr) {
            c_flags_ |= HASHMAP_FLAG_ARENA_STORAGE;
            core_.set_storage_kind(
                ::memkit::detail::hashmap_storage_kind::owns |
                ::memkit::detail::hashmap_storage_kind::arena
            );
        }
#if MEMKIT_ALLOW_HEAP
        else {
            c_flags_ |= HASHMAP_FLAG_DYNAMIC_STORAGE;
            core_.set_storage_kind(
                ::memkit::detail::hashmap_storage_kind::owns |
                ::memkit::detail::hashmap_storage_kind::heap
            );
        }
#endif
        return HASHMAP_OK;
    }

    [[nodiscard]] hashmap_status_t alloc_raw(
        std::size_t bytes,
        std::size_t alignment,
        void** out_ptr
    ) noexcept
    {
        if (arena_ != nullptr) {
            const arena_status_t status = arena_alloc(arena_, bytes, alignment, out_ptr);
            if (!arena_status_ok(status)) {
                return status == ARENA_ERR_OOM ? HASHMAP_ERR_OOM : HASHMAP_ERR_INVALID;
            }
            return HASHMAP_OK;
        }

#if MEMKIT_ALLOW_HEAP
        void* const ptr = std::malloc(bytes);
        if (ptr == nullptr) {
            return HASHMAP_ERR_OOM;
        }
        *out_ptr = ptr;
        return HASHMAP_OK;
#else
        (void)bytes;
        (void)alignment;
        (void)out_ptr;
        return HASHMAP_ERR_OOM;
#endif
    }

    void release_bucket_storage() noexcept
    {
        if ((c_flags_ & HASHMAP_FLAG_OWNS_STORAGE) == 0u) {
            return;
        }

#if MEMKIT_ALLOW_HEAP
        if ((c_flags_ & HASHMAP_FLAG_DYNAMIC_STORAGE) != 0u) {
            core_.release_storage();
        }
#endif

        core_.set_storage_kind(::memkit::detail::hashmap_storage_kind::external);
    }

    ::memkit::detail::hashmap_map_core core_{};
    unsigned                         c_flags_ = 0u;
    arena_t*                         arena_   = nullptr;
};

static_assert(sizeof(hashmap_box) <= MEMKIT_HASHMAP_OBJ_BYTES);

} // namespace memkit::c_api
