#pragma once

#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <cstring>

namespace memkit {

/** Fixed-capacity length-prefixed byte buffer for protocol payloads (no heap). */
template<std::size_t N>
class SmallBuffer {
public:
    static constexpr std::size_t capacity = N;

    SmallBuffer() noexcept { clear(); }

    explicit SmallBuffer(stl::const_byte_span bytes) noexcept { (void)assign(bytes); }

    [[nodiscard]] std::size_t size() const noexcept { return len_; }
    [[nodiscard]] bool empty() const noexcept { return len_ == 0u; }
    [[nodiscard]] bool full() const noexcept { return len_ >= N; }

    [[nodiscard]] const std::byte* data() const noexcept { return buf_; }
    [[nodiscard]] std::byte* data() noexcept { return buf_; }

    [[nodiscard]] stl::const_byte_span view() const noexcept
    {
        return stl::const_byte_span{buf_, len_};
    }

    [[nodiscard]] stl::byte_span mutable_view() noexcept
    {
        return stl::byte_span{buf_, len_};
    }

    void clear() noexcept { len_ = 0u; }

    [[nodiscard]] status assign(stl::const_byte_span bytes) noexcept
    {
        if (bytes.size() > N) {
            return status::invalid;
        }

        if (bytes.empty()) {
            clear();
            return status::ok;
        }

        std::memcpy(buf_, bytes.data(), bytes.size());
        len_ = bytes.size();
        return status::ok;
    }

    [[nodiscard]] status assign(const void* data, std::size_t count) noexcept
    {
        if (data == nullptr && count > 0u) {
            return status::null_ptr;
        }
        if (count == 0u) {
            clear();
            return status::ok;
        }

        return assign(stl::const_byte_span{
            static_cast<const std::byte*>(data),
            count
        });
    }

    [[nodiscard]] status append(stl::const_byte_span bytes) noexcept
    {
        if (bytes.size() + len_ > N) {
            return status::full;
        }
        if (bytes.empty()) {
            return status::ok;
        }

        std::memcpy(buf_ + len_, bytes.data(), bytes.size());
        len_ += bytes.size();
        return status::ok;
    }

    [[nodiscard]] status append(std::byte byte) noexcept
    {
        if (len_ >= N) {
            return status::full;
        }

        buf_[len_] = byte;
        ++len_;
        return status::ok;
    }

    [[nodiscard]] status push_back(std::byte byte) noexcept { return append(byte); }

    [[nodiscard]] status resize(std::size_t new_size, std::byte fill = std::byte{0}) noexcept
    {
        if (new_size > N) {
            return status::invalid;
        }

        if (new_size > len_) {
            std::memset(buf_ + len_, static_cast<unsigned char>(fill), new_size - len_);
        }

        len_ = new_size;
        return status::ok;
    }

    [[nodiscard]] bool operator==(stl::const_byte_span other) const noexcept
    {
        if (len_ != other.size()) {
            return false;
        }
        return std::memcmp(buf_, other.data(), len_) == 0;
    }

    [[nodiscard]] bool operator!=(stl::const_byte_span other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] bool operator==(const SmallBuffer& other) const noexcept
    {
        return *this == other.view();
    }

    [[nodiscard]] bool operator!=(const SmallBuffer& other) const noexcept
    {
        return !(*this == other);
    }

private:
    std::byte     buf_[N]{};
    std::size_t   len_ = 0u;
};

} // namespace memkit
