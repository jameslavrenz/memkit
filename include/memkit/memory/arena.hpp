#pragma once

#include "../config.hpp"
#include "../detail/utility.hpp"
#include "fixed_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace memkit::memory {

template<typename BackingStorage>
class arena {
public:
    using backing_type = BackingStorage;

    arena() noexcept = default;

    explicit arena(BackingStorage backing) noexcept
        : backing_{std::move(backing)}
    {}

    [[nodiscard]] std::byte* base() noexcept
    {
        return backing_.data();
    }

    [[nodiscard]] const std::byte* base() const noexcept
    {
        return backing_.data();
    }

    [[nodiscard]] std::size_t capacity_bytes() const noexcept
    {
        return backing_.size();
    }

    [[nodiscard]] std::size_t used_bytes() const noexcept
    {
        return offset_bytes_;
    }

    [[nodiscard]] std::size_t remaining_bytes() const noexcept
    {
        return capacity_bytes() - offset_bytes_;
    }

    [[nodiscard]] std::size_t allocation_count() const noexcept
    {
        return allocation_count_;
    }

    void reset() noexcept
    {
        offset_bytes_     = 0u;
        allocation_count_ = 0u;
    }

    [[nodiscard]] status allocate(std::size_t size, std::size_t alignment, void** out_ptr)
    {
        if (out_ptr == nullptr) {
            return status::null_ptr;
        }
        *out_ptr = nullptr;

        if (base() == nullptr || capacity_bytes() == 0u) {
            return status::null_ptr;
        }
        if (size == 0u || alignment == 0u || !detail::is_power_of_two(alignment)) {
            return status::invalid;
        }

        const std::size_t aligned_offset = detail::align_up(offset_bytes_, alignment);
        if (aligned_offset > capacity_bytes() ||
            size > capacity_bytes() - aligned_offset) {
            return status::oom;
        }

        *out_ptr = base() + aligned_offset;
        offset_bytes_ = aligned_offset + size;
        ++allocation_count_;
        return status::ok;
    }

    [[nodiscard]] status calloc(
        std::size_t count,
        std::size_t size,
        std::size_t alignment,
        void** out_ptr
    )
    {
        if (count != 0u && size > SIZE_MAX / count) {
            return status::invalid;
        }

        const std::size_t total = count * size;
        const status st = allocate(total, alignment, out_ptr);
        if (!ok(st)) {
            return st;
        }

        std::memset(*out_ptr, 0, total);
        return status::ok;
    }

    [[nodiscard]] BackingStorage&       backing()       noexcept { return backing_; }
    [[nodiscard]] const BackingStorage& backing() const noexcept { return backing_; }

private:
    BackingStorage backing_{};
    std::size_t    offset_bytes_     = 0u;
    std::size_t    allocation_count_ = 0u;
};

} // namespace memkit::memory
