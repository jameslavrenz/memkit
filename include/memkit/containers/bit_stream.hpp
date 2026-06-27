#pragma once

#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <cstdint>

namespace memkit {

/** MSB-first bit reader over an external byte buffer (CAN, packed structs). */
class BitReader {
public:
    BitReader() noexcept = default;

    [[nodiscard]] status init(stl::const_byte_span bytes) noexcept
    {
        if (bytes.data() == nullptr && bytes.size() > 0u) {
            return status::null_ptr;
        }

        bytes_   = bytes;
        bit_pos_ = 0u;
        return status::ok;
    }

    [[nodiscard]] std::size_t bit_position() const noexcept { return bit_pos_; }

    [[nodiscard]] std::size_t bits_remaining() const noexcept
    {
        const std::size_t total_bits = bytes_.size() * 8u;
        return bit_pos_ >= total_bits ? 0u : total_bits - bit_pos_;
    }

    [[nodiscard]] bool empty() const noexcept { return bits_remaining() == 0u; }

    void reset() noexcept { bit_pos_ = 0u; }

    [[nodiscard]] status read_bit(bool& out) noexcept
    {
        std::uint32_t bits = 0u;
        const status st  = read_bits(1u, bits);
        if (!ok(st)) {
            return st;
        }

        out = (bits & 1u) != 0u;
        return status::ok;
    }

    [[nodiscard]] status read_bits(std::uint8_t count, std::uint32_t& out) noexcept
    {
        if (count == 0u || count > 32u) {
            return status::invalid;
        }
        if (count > bits_remaining()) {
            return status::empty;
        }

        std::uint32_t value = 0u;
        for (std::uint8_t i = 0u; i < count; ++i) {
            const std::size_t absolute_bit = bit_pos_ + static_cast<std::size_t>(i);
            const std::size_t byte_index   = absolute_bit / 8u;
            const std::size_t bit_index    = 7u - (absolute_bit % 8u);
            const auto byte                = static_cast<unsigned char>(bytes_[byte_index]);
            const std::uint32_t bit        = (byte >> bit_index) & 1u;
            value                          = (value << 1u) | bit;
        }

        bit_pos_ += static_cast<std::size_t>(count);
        out = value;
        return status::ok;
    }

    [[nodiscard]] status skip_bits(std::size_t count) noexcept
    {
        if (count > bits_remaining()) {
            return status::empty;
        }

        bit_pos_ += count;
        return status::ok;
    }

    [[nodiscard]] status align_to_byte() noexcept
    {
        const std::size_t offset = bit_pos_ % 8u;
        if (offset == 0u) {
            return status::ok;
        }
        return skip_bits(8u - offset);
    }

private:
    stl::const_byte_span bytes_{};
    std::size_t          bit_pos_ = 0u;
};

/** MSB-first bit writer into an external byte buffer. */
class BitWriter {
public:
    BitWriter() noexcept = default;

    [[nodiscard]] status init(stl::byte_span bytes) noexcept
    {
        if (bytes.data() == nullptr && bytes.size() > 0u) {
            return status::null_ptr;
        }

        bytes_   = bytes;
        bit_pos_ = 0u;
        if (!bytes.empty()) {
            for (std::size_t i = 0u; i < bytes.size(); ++i) {
                bytes[i] = std::byte{0};
            }
        }
        return status::ok;
    }

    [[nodiscard]] std::size_t bit_position() const noexcept { return bit_pos_; }

    [[nodiscard]] std::size_t capacity_bits() const noexcept { return bytes_.size() * 8u; }

    [[nodiscard]] std::size_t bits_available() const noexcept
    {
        const std::size_t cap = capacity_bits();
        return bit_pos_ >= cap ? 0u : cap - bit_pos_;
    }

    [[nodiscard]] std::size_t byte_length() const noexcept
    {
        return (bit_pos_ + 7u) / 8u;
    }

    void reset() noexcept
    {
        bit_pos_ = 0u;
        for (std::size_t i = 0u; i < bytes_.size(); ++i) {
            bytes_[i] = std::byte{0};
        }
    }

    [[nodiscard]] status write_bit(bool value) noexcept
    {
        return write_bits(value ? 1u : 0u, 1u);
    }

    [[nodiscard]] status write_bits(std::uint32_t value, std::uint8_t count) noexcept
    {
        if (count == 0u || count > 32u) {
            return status::invalid;
        }
        if (static_cast<std::size_t>(count) > bits_available()) {
            return status::full;
        }

        for (std::int8_t i = static_cast<std::int8_t>(count) - 1; i >= 0; --i) {
            const bool bit = ((value >> static_cast<unsigned>(i)) & 1u) != 0u;
            const std::size_t absolute_bit = bit_pos_;
            const std::size_t byte_index   = absolute_bit / 8u;
            const std::size_t bit_index    = 7u - (absolute_bit % 8u);
            auto& byte                     = bytes_[byte_index];
            const auto mask                = static_cast<unsigned char>(1u << bit_index);
            if (bit) {
                byte = static_cast<std::byte>(static_cast<unsigned char>(byte) | mask);
            } else {
                byte = static_cast<std::byte>(static_cast<unsigned char>(byte) & ~mask);
            }
            ++bit_pos_;
        }

        return status::ok;
    }

    [[nodiscard]] status align_to_byte() noexcept
    {
        const std::size_t offset = bit_pos_ % 8u;
        if (offset == 0u) {
            return status::ok;
        }
        return write_bits(0u, static_cast<std::uint8_t>(8u - offset));
    }

private:
    stl::byte_span bytes_{};
    std::size_t    bit_pos_ = 0u;
};

} // namespace memkit
