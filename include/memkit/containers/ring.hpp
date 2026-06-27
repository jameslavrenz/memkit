#pragma once

#include "../detail/element_policy.hpp"
#include "../detail/ring_core.hpp"
#include "../detail/storage_view.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace memkit {

enum class ring_policy : unsigned {
    none              = 0u,
    overwrite_on_full = 1u << 0u,
};

template<typename T>
class Ring {
public:
    Ring() noexcept = default;

    Ring(Ring&& other) noexcept
        : core_{std::move(other.core_)}
        , owns_storage_{std::exchange(other.owns_storage_, false)}
    {}

    Ring& operator=(Ring&& other) noexcept
    {
        if (this != &other) {
            clear();
            release_storage();
            core_         = std::move(other.core_);
            owns_storage_ = std::exchange(other.owns_storage_, false);
        }
        return *this;
    }

    Ring(const Ring&)            = delete;
    Ring& operator=(const Ring&) = delete;

    ~Ring() { clear(); release_storage(); }

    [[nodiscard]] status init(std::byte* storage, std::size_t capacity) noexcept
    {
        detail::typed_element_policy<T> policy{};
        return core_.init(policy, storage, capacity, detail::ring_policy::none);
    }

    [[nodiscard]] status init(stl::byte_span storage, std::size_t capacity) noexcept
    {
        if (!detail::byte_span_fits(storage, sizeof(T), capacity)) {
            return status::invalid;
        }
        return init(storage.data(), capacity);
    }

    template<std::size_t N>
    [[nodiscard]] status init(stl::array<std::byte, N>& storage, std::size_t capacity) noexcept
    {
        return init(stl::byte_span{storage.data(), N}, capacity);
    }

    template<std::size_t N>
    [[nodiscard]] status init(stl::array<T, N>& storage) noexcept
    {
        return init(stl::span<T>{storage.data(), N});
    }

    [[nodiscard]] status init(stl::span<T> storage) noexcept
    {
        return init(detail::object_bytes(storage), storage.size());
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t capacity,
        ring_policy policy = ring_policy::none
    )
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(sizeof(T) * capacity, alignof(T), &ptr);
        if (!ok(st)) {
            return st;
        }

        detail::typed_element_policy<T> ep{};
        return core_.init(
            ep,
            static_cast<std::byte*>(ptr),
            capacity,
            to_detail_policy(policy)
        );
    }

    void set_policy(ring_policy policy) noexcept
    {
        (void)policy;
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status push_back(const T& value)
    {
        return core_.push_back(&value, has_overwrite_on_full(), false);
    }

    [[nodiscard]] status push_back(T&& value)
    {
        return core_.push_back(&value, has_overwrite_on_full(), true);
    }

    [[nodiscard]] status pop_front(T* out = nullptr)
    {
        return core_.pop_front(out != nullptr ? static_cast<void*>(out) : nullptr);
    }

    [[nodiscard]] status peek_front(T& out) const
    {
        return core_.peek_front(static_cast<void*>(&out));
    }

    [[nodiscard]] status peek_back(T& out) const
    {
        return core_.peek_back(static_cast<void*>(&out));
    }

    [[nodiscard]] stl::optional<T> try_pop_front()
    {
        T value{};
        const status st = pop_front(&value);
        if (!ok(st)) {
            return stl::nullopt;
        }
        return value;
    }

    [[nodiscard]] stl::optional<T> try_peek_front() const
    {
        T value{};
        const status st = peek_front(value);
        if (!ok(st)) {
            return stl::nullopt;
        }
        return value;
    }

    [[nodiscard]] stl::optional<T> try_peek_back() const
    {
        T value{};
        const status st = peek_back(value);
        if (!ok(st)) {
            return stl::nullopt;
        }
        return value;
    }

    [[nodiscard]] const T* data_at(std::size_t index) const noexcept
    {
        if (index >= core_.size()) {
            return nullptr;
        }
        return static_cast<const T*>(core_.logical_slot(index));
    }

    [[nodiscard]] std::size_t readable_contiguous(const T** out_ptr) const noexcept
    {
        const void* ptr = nullptr;
        const std::size_t n = core_.readable_contiguous(&ptr);
        if (out_ptr != nullptr) {
            *out_ptr = n > 0u ? static_cast<const T*>(ptr) : nullptr;
        }
        return n;
    }

    [[nodiscard]] std::size_t writable_contiguous(T** out_ptr) noexcept
    {
        void* ptr = nullptr;
        const std::size_t n = core_.writable_contiguous(&ptr);
        if (out_ptr != nullptr) {
            *out_ptr = n > 0u ? static_cast<T*>(ptr) : nullptr;
        }
        return n;
    }

    void commit_read(std::size_t n) noexcept { core_.commit_read(n); }
    void commit_write(std::size_t n) noexcept { core_.commit_write(n); }

private:
    [[nodiscard]] static detail::ring_policy to_detail_policy(ring_policy policy) noexcept
    {
        if ((static_cast<unsigned>(policy) &
             static_cast<unsigned>(ring_policy::overwrite_on_full)) != 0u) {
            return detail::ring_policy::overwrite_on_full;
        }
        return detail::ring_policy::none;
    }

    [[nodiscard]] bool has_overwrite_on_full() const noexcept
    {
        return detail::has(core_.flags(), detail::ring_policy::overwrite_on_full);
    }

    void release_storage() noexcept
    {
        if (owns_storage_ && core_.storage() != nullptr) {
            if constexpr (allow_heap) {
                std::free(core_.storage());
            }
        }
        owns_storage_ = false;
    }

    detail::ring_core<detail::typed_element_policy<T>> core_{};
    bool                                               owns_storage_ = false;
};

} // namespace memkit
