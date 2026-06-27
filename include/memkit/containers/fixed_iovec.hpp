#pragma once

#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>

namespace memkit {

struct IoSlice {
    const std::byte* data = nullptr;
    std::size_t      size = 0u;
};

struct MutableIoSlice {
    std::byte*  data = nullptr;
    std::size_t size = 0u;
};

/** Fixed-capacity scatter/gather slice list for DMA and zero-copy I/O. */
template<std::size_t MaxSlices>
class FixedIoVec {
public:
    static constexpr std::size_t max_slices = MaxSlices;

    [[nodiscard]] std::size_t slice_count() const noexcept { return count_; }

    [[nodiscard]] bool empty() const noexcept { return count_ == 0u; }

    [[nodiscard]] bool full() const noexcept { return count_ >= MaxSlices; }

    [[nodiscard]] std::size_t total_bytes() const noexcept
    {
        std::size_t total = 0u;
        for (std::size_t i = 0u; i < count_; ++i) {
            total += slices_[i].size;
        }
        return total;
    }

    void clear() noexcept { count_ = 0u; }

    [[nodiscard]] status push(const void* data, std::size_t size) noexcept
    {
        if (data == nullptr && size > 0u) {
            return status::null_ptr;
        }
        if (full()) {
            return status::full;
        }

        slices_[count_].data = static_cast<const std::byte*>(data);
        slices_[count_].size = size;
        ++count_;
        return status::ok;
    }

    [[nodiscard]] status push(stl::const_byte_span bytes) noexcept
    {
        return push(bytes.data(), bytes.size());
    }

    [[nodiscard]] status push(MutableIoSlice slice) noexcept
    {
        return push(static_cast<const void*>(slice.data), slice.size);
    }

    [[nodiscard]] const IoSlice& operator[](std::size_t index) const noexcept
    {
        return slices_[index];
    }

    [[nodiscard]] stl::span<const IoSlice> slices() const noexcept
    {
        return stl::span<const IoSlice>{slices_.data(), count_};
    }

private:
    stl::array<IoSlice, MaxSlices> slices_{};
    std::size_t                    count_ = 0u;
};

} // namespace memkit
