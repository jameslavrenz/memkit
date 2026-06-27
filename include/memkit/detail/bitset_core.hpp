#pragma once

#include "../status.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace memkit::detail {

enum class bitset_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline bitset_storage_kind operator|(
    bitset_storage_kind a,
    bitset_storage_kind b
) noexcept
{
    return static_cast<bitset_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

class bitset_core {
public:
    bitset_core() = default;

    [[nodiscard]] static constexpr std::size_t storage_bytes(std::size_t capacity) noexcept
    {
        return (capacity + 7u) / 8u;
    }

    [[nodiscard]] static constexpr std::uint8_t tail_mask(std::size_t capacity) noexcept
    {
        const std::size_t remainder = capacity % 8u;
        if (remainder == 0u) {
            return 0xFFu;
        }
        return static_cast<std::uint8_t>((1u << remainder) - 1u);
    }

    [[nodiscard]] status init(std::uint8_t* storage, std::size_t capacity) noexcept
    {
        if (storage == nullptr || capacity == 0u) {
            return status::invalid;
        }

        storage_       = storage;
        capacity_      = capacity;
        storage_bytes_ = storage_bytes(capacity);
        storage_kind_  = bitset_storage_kind::external;
        clear();
        return status::ok;
    }

    void reset_state() noexcept
    {
        storage_       = nullptr;
        capacity_      = 0u;
        storage_bytes_ = 0u;
        storage_kind_  = bitset_storage_kind::external;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t storage_byte_count() const noexcept { return storage_bytes_; }
    [[nodiscard]] bitset_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] std::uint8_t* storage() const noexcept { return storage_; }

    void set_storage_kind(bitset_storage_kind kind) noexcept { storage_kind_ = kind; }

    [[nodiscard]] std::size_t size() const noexcept
    {
        if (storage_ == nullptr || storage_bytes_ == 0u) {
            return 0u;
        }

        std::size_t count = 0u;
        for (std::size_t i = 0u; i + 1u < storage_bytes_; ++i) {
            count += popcount_byte(storage_[i]);
        }
        count += popcount_byte(storage_[storage_bytes_ - 1u]);
        return count;
    }

    [[nodiscard]] bool empty() const noexcept { return size() == 0u; }
    [[nodiscard]] bool full() const noexcept { return size() == capacity_; }

    void clear() noexcept
    {
        if (storage_ != nullptr) {
            std::memset(storage_, 0, storage_bytes_);
        }
    }

    [[nodiscard]] status set_all() noexcept
    {
        if (storage_ == nullptr) {
            return status::null_ptr;
        }

        std::memset(storage_, 0xFF, storage_bytes_);
        apply_tail_mask();
        return status::ok;
    }

    [[nodiscard]] bool test(std::size_t index) const noexcept
    {
        if (index >= capacity_ || storage_ == nullptr) {
            return false;
        }
        return (storage_[index / 8u] & byte_mask(index)) != 0u;
    }

    [[nodiscard]] status set(std::size_t index) noexcept
    {
        if (storage_ == nullptr) {
            return status::null_ptr;
        }
        if (index >= capacity_) {
            return status::invalid;
        }
        storage_[index / 8u] |= byte_mask(index);
        return status::ok;
    }

    [[nodiscard]] status reset(std::size_t index) noexcept
    {
        if (storage_ == nullptr) {
            return status::null_ptr;
        }
        if (index >= capacity_) {
            return status::invalid;
        }
        storage_[index / 8u] &= static_cast<std::uint8_t>(~byte_mask(index));
        return status::ok;
    }

    [[nodiscard]] status toggle(std::size_t index) noexcept
    {
        if (storage_ == nullptr) {
            return status::null_ptr;
        }
        if (index >= capacity_) {
            return status::invalid;
        }
        storage_[index / 8u] ^= byte_mask(index);
        return status::ok;
    }

    [[nodiscard]] status assign(bool value, std::size_t index) noexcept
    {
        return value ? set(index) : reset(index);
    }

    [[nodiscard]] status find_first_set(std::size_t start_index, std::size_t& out_index) const noexcept
    {
        return find_from(start_index, true, out_index);
    }

    [[nodiscard]] status find_first_clear(std::size_t start_index, std::size_t& out_index) const noexcept
    {
        return find_from(start_index, false, out_index);
    }

    [[nodiscard]] status copy_from(const bitset_core& src) noexcept
    {
        if (storage_ == nullptr || src.storage_ == nullptr) {
            return status::invalid;
        }
        if (capacity_ != src.capacity_) {
            return status::invalid;
        }

        std::memcpy(storage_, src.storage_, storage_bytes_);
        apply_tail_mask();
        return status::ok;
    }

    [[nodiscard]] bool equal(const bitset_core& other) const noexcept
    {
        if (capacity_ != other.capacity_ || storage_ == nullptr || other.storage_ == nullptr) {
            return false;
        }
        return std::memcmp(storage_, other.storage_, storage_bytes_) == 0;
    }

    [[nodiscard]] status union_with(const bitset_core& other) noexcept
    {
        return combine_with(other, [](std::uint8_t a, std::uint8_t b) { return a | b; });
    }

    [[nodiscard]] status intersect_with(const bitset_core& other) noexcept
    {
        return combine_with(other, [](std::uint8_t a, std::uint8_t b) { return a & b; });
    }

    [[nodiscard]] status xor_with(const bitset_core& other) noexcept
    {
        return combine_with(other, [](std::uint8_t a, std::uint8_t b) { return a ^ b; });
    }

    [[nodiscard]] status complement() noexcept
    {
        if (storage_ == nullptr) {
            return status::null_ptr;
        }

        for (std::size_t i = 0u; i < storage_bytes_; ++i) {
            storage_[i] = static_cast<std::uint8_t>(~storage_[i]);
        }
        apply_tail_mask();
        return status::ok;
    }

    [[nodiscard]] status load_bytes(const std::byte* bytes, std::size_t bytes_len) noexcept
    {
        if (storage_ == nullptr || bytes == nullptr) {
            return status::null_ptr;
        }
        if (bytes_len < storage_bytes_) {
            return status::invalid;
        }

        std::memcpy(storage_, bytes, storage_bytes_);
        apply_tail_mask();
        return status::ok;
    }

    [[nodiscard]] status store_bytes(std::byte* bytes, std::size_t bytes_len) const noexcept
    {
        if (storage_ == nullptr || bytes == nullptr) {
            return status::null_ptr;
        }
        if (bytes_len < storage_bytes_) {
            return status::invalid;
        }

        std::memcpy(bytes, storage_, storage_bytes_);
        return status::ok;
    }

    [[nodiscard]] std::uint8_t* data() noexcept { return storage_; }
    [[nodiscard]] const std::uint8_t* data() const noexcept { return storage_; }

    template<typename Visitor>
    [[nodiscard]] status for_each(Visitor&& visit) const
    {
        if (storage_ == nullptr) {
            return status::null_ptr;
        }

        for (std::size_t index = 0u; index < capacity_; ++index) {
            if (!test(index)) {
                continue;
            }
            if constexpr (std::is_same_v<std::invoke_result_t<Visitor, std::size_t>, status>) {
                const status st = visit(index);
                if (!ok(st)) {
                    return st;
                }
            } else {
                visit(index);
            }
        }

        return status::ok;
    }

private:
    [[nodiscard]] static constexpr std::uint8_t byte_mask(std::size_t index) noexcept
    {
        return static_cast<std::uint8_t>(1u << (index % 8u));
    }

    [[nodiscard]] static std::size_t popcount_byte(std::uint8_t value) noexcept
    {
        std::size_t count = 0u;
        while (value != 0u) {
            count += static_cast<std::size_t>(value & 1u);
            value = static_cast<std::uint8_t>(value >> 1u);
        }
        return count;
    }

    void apply_tail_mask() noexcept
    {
        if (storage_ != nullptr && storage_bytes_ > 0u) {
            storage_[storage_bytes_ - 1u] &= tail_mask(capacity_);
        }
    }

    [[nodiscard]] status find_from(
        std::size_t start_index,
        bool want_set,
        std::size_t& out_index
    ) const noexcept
    {
        if (storage_ == nullptr) {
            return status::null_ptr;
        }
        if (start_index >= capacity_) {
            return status::not_found;
        }

        for (std::size_t index = start_index; index < capacity_; ++index) {
            if (test(index) == want_set) {
                out_index = index;
                return status::ok;
            }
        }

        return status::not_found;
    }

    template<typename CombineFn>
    [[nodiscard]] status combine_with(const bitset_core& other, CombineFn combine) noexcept
    {
        if (storage_ == nullptr || other.storage_ == nullptr) {
            return status::invalid;
        }
        if (capacity_ != other.capacity_) {
            return status::invalid;
        }

        for (std::size_t i = 0u; i < storage_bytes_; ++i) {
            storage_[i] = combine(storage_[i], other.storage_[i]);
        }
        apply_tail_mask();
        return status::ok;
    }

    std::uint8_t*         storage_       = nullptr;
    std::size_t           capacity_      = 0u;
    std::size_t           storage_bytes_ = 0u;
    bitset_storage_kind   storage_kind_  = bitset_storage_kind::external;
};

} // namespace memkit::detail
