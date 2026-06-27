#pragma once

#include "../config.hpp"
#include "../detail/element_policy.hpp"
#include "../detail/objpool_core.hpp"
#include "../detail/utility.hpp"
#include "../status.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace memkit {

template<typename T>
class ObjPool {
public:
    ObjPool() noexcept = default;

    ObjPool(ObjPool&& other) noexcept
        : core_{std::move(other.core_)}
        , owns_storage_{std::exchange(other.owns_storage_, false)}
        , owns_meta_{std::exchange(other.owns_meta_, false)}
    {}

    ObjPool& operator=(ObjPool&& other) noexcept
    {
        if (this != &other) {
            clear();
            release_owned();
            core_         = std::move(other.core_);
            owns_storage_ = std::exchange(other.owns_storage_, false);
            owns_meta_    = std::exchange(other.owns_meta_, false);
        }
        return *this;
    }

    ObjPool(const ObjPool&)            = delete;
    ObjPool& operator=(const ObjPool&) = delete;

    ~ObjPool() { clear(); release_owned(); }

    [[nodiscard]] static constexpr std::size_t used_bits_bytes(std::size_t capacity) noexcept
    {
        return detail::objpool_core<detail::typed_element_policy<T>>::used_bits_bytes(capacity);
    }

    [[nodiscard]] static constexpr std::size_t free_stack_bytes(std::size_t capacity) noexcept
    {
        return detail::objpool_core<detail::typed_element_policy<T>>::free_stack_bytes(capacity);
    }

    [[nodiscard]] static constexpr std::size_t storage_bytes(std::size_t capacity) noexcept
    {
        return sizeof(T) * capacity;
    }

    [[nodiscard]] status init(
        std::byte* storage,
        std::uint32_t* free_stack,
        std::byte* used_bits,
        std::size_t capacity
    ) noexcept
    {
        detail::typed_element_policy<T> policy{};
        return core_.init(policy, storage, free_stack, used_bits, capacity);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t capacity)
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        void* storage_ptr = nullptr;
        status st = arena.allocate(storage_bytes(capacity), alignof(T), &storage_ptr);
        if (!ok(st)) {
            return st;
        }

        void* stack_ptr = nullptr;
        st = arena.allocate(free_stack_bytes(capacity), alignof(std::uint32_t), &stack_ptr);
        if (!ok(st)) {
            return st;
        }

        void* bits_ptr = nullptr;
        st = arena.allocate(used_bits_bytes(capacity), alignof(std::uint8_t), &bits_ptr);
        if (!ok(st)) {
            return st;
        }

        detail::typed_element_policy<T> policy{};
        return core_.init(
            policy,
            static_cast<std::byte*>(storage_ptr),
            static_cast<std::uint32_t*>(stack_ptr),
            static_cast<std::byte*>(bits_ptr),
            capacity
        );
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] std::size_t available() const noexcept { return core_.available(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status alloc(T** out_elem) noexcept
    {
        void* elem = nullptr;
        const status st = core_.alloc(&elem);
        if (!ok(st)) {
            return st;
        }
        *out_elem = static_cast<T*>(elem);
        return status::ok;
    }

    [[nodiscard]] status alloc_copy(const T& src, T** out_elem)
    {
        void* elem = nullptr;
        const status st = core_.alloc_copy(&src, &elem);
        if (!ok(st)) {
            return st;
        }
        *out_elem = static_cast<T*>(elem);
        return status::ok;
    }

    [[nodiscard]] status index(const T* elem, std::size_t& out_index) const noexcept
    {
        return core_.index(elem, out_index);
    }

    [[nodiscard]] bool contains(const T* elem) const noexcept { return core_.contains(elem); }

    [[nodiscard]] status free(T* elem) noexcept { return core_.free(elem); }

    template<typename Visitor>
    [[nodiscard]] status for_each(Visitor&& visit) const
    {
        return core_.for_each([&visit](const void* elem, std::size_t index) {
            if constexpr (std::is_same_v<
                              std::invoke_result_t<Visitor, const T&, std::size_t>,
                              status>) {
                return visit(*static_cast<const T*>(elem), index);
            }
            visit(*static_cast<const T*>(elem), index);
            return status::ok;
        });
    }

private:
    void release_owned() noexcept
    {
        if (owns_storage_ && core_.storage() != nullptr) {
#if MEMKIT_ALLOW_HEAP
            std::free(core_.storage());
#endif
        }
        if (owns_meta_) {
#if MEMKIT_ALLOW_HEAP
            if (core_.free_stack() != nullptr) {
                std::free(core_.free_stack());
            }
            if (core_.used_bits() != nullptr) {
                std::free(core_.used_bits());
            }
#endif
        }
        owns_storage_ = false;
        owns_meta_    = false;
    }

    detail::objpool_core<detail::typed_element_policy<T>> core_{};
    bool                                                  owns_storage_ = false;
    bool                                                  owns_meta_    = false;
};

} // namespace memkit
