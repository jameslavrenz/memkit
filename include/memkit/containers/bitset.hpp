#pragma once

#include "../config.hpp"
#include "../detail/bitset_core.hpp"
#include "../detail/storage_view.hpp"
#include "../detail/utility.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace memkit {

class Bitset {
public:
    Bitset() noexcept = default;

    Bitset(Bitset&& other) noexcept
        : core_{std::move(other.core_)}
        , owns_storage_{std::exchange(other.owns_storage_, false)}
    {}

    Bitset& operator=(Bitset&& other) noexcept
    {
        if (this != &other) {
            release_storage();
            core_         = std::move(other.core_);
            owns_storage_ = std::exchange(other.owns_storage_, false);
        }
        return *this;
    }

    Bitset(const Bitset&)            = delete;
    Bitset& operator=(const Bitset&) = delete;

    ~Bitset() { release_storage(); }

    [[nodiscard]] static constexpr std::size_t storage_bytes(std::size_t capacity) noexcept
    {
        return detail::bitset_core::storage_bytes(capacity);
    }

    [[nodiscard]] static constexpr std::uint8_t tail_mask(std::size_t capacity) noexcept
    {
        return detail::bitset_core::tail_mask(capacity);
    }

    [[nodiscard]] status init(std::byte* storage, std::size_t capacity) noexcept
    {
        return core_.init(reinterpret_cast<std::uint8_t*>(storage), capacity);
    }

    [[nodiscard]] status init(stl::byte_span storage, std::size_t capacity) noexcept
    {
        if (storage.size() < storage_bytes(capacity)) {
            return status::invalid;
        }
        return init(storage.data(), capacity);
    }

    template<std::size_t N>
    [[nodiscard]] status init(stl::array<std::byte, N>& storage, std::size_t capacity) noexcept
    {
        return init(stl::byte_span{storage.data(), N}, capacity);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t capacity)
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(storage_bytes(capacity), alignof(std::uint8_t), &ptr);
        if (!ok(st)) {
            return st;
        }

        return core_.init(static_cast<std::uint8_t*>(ptr), capacity);
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status set_all() noexcept { return core_.set_all(); }
    [[nodiscard]] bool test(std::size_t index) const noexcept { return core_.test(index); }
    [[nodiscard]] status set(std::size_t index) noexcept { return core_.set(index); }
    [[nodiscard]] status reset(std::size_t index) noexcept { return core_.reset(index); }
    [[nodiscard]] status toggle(std::size_t index) noexcept { return core_.toggle(index); }
    [[nodiscard]] status assign(bool value, std::size_t index) noexcept
    {
        return core_.assign(value, index);
    }

    [[nodiscard]] status find_first_set(std::size_t start_index, std::size_t& out_index) const noexcept
    {
        return core_.find_first_set(start_index, out_index);
    }

    [[nodiscard]] status find_first_clear(std::size_t start_index, std::size_t& out_index) const noexcept
    {
        return core_.find_first_clear(start_index, out_index);
    }

    [[nodiscard]] status copy_from(const Bitset& src) noexcept { return core_.copy_from(src.core_); }
    [[nodiscard]] bool equal(const Bitset& other) const noexcept { return core_.equal(other.core_); }
    [[nodiscard]] status union_with(const Bitset& other) noexcept { return core_.union_with(other.core_); }
    [[nodiscard]] status intersect_with(const Bitset& other) noexcept
    {
        return core_.intersect_with(other.core_);
    }
    [[nodiscard]] status xor_with(const Bitset& other) noexcept { return core_.xor_with(other.core_); }
    [[nodiscard]] status complement() noexcept { return core_.complement(); }

    [[nodiscard]] status load_bytes(const std::byte* bytes, std::size_t bytes_len) noexcept
    {
        return core_.load_bytes(bytes, bytes_len);
    }

    [[nodiscard]] status store_bytes(std::byte* bytes, std::size_t bytes_len) const noexcept
    {
        return core_.store_bytes(bytes, bytes_len);
    }

    [[nodiscard]] std::uint8_t* data() noexcept { return core_.data(); }
    [[nodiscard]] const std::uint8_t* data() const noexcept { return core_.data(); }
    [[nodiscard]] std::size_t data_bytes() const noexcept { return core_.storage_byte_count(); }

    template<typename Visitor>
    [[nodiscard]] status for_each(Visitor&& visit) const
    {
        return core_.for_each(std::forward<Visitor>(visit));
    }

private:
    void release_storage() noexcept
    {
        if (owns_storage_ && core_.storage() != nullptr) {
#if MEMKIT_ALLOW_HEAP
            std::free(core_.storage());
#endif
        }
        owns_storage_ = false;
    }

    detail::bitset_core core_{};
    bool                owns_storage_ = false;
};

} // namespace memkit
