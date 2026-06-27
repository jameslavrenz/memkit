#pragma once

#include "../detail/ring_core.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace memkit {

enum class byte_ring_policy : unsigned {
    none              = 0u,
    overwrite_on_full = 1u << 0u,
};

/** Byte-oriented ring buffer for UART/DMA/stream I/O (wraps ring_buffer_core). */
class ByteRing {
public:
    ByteRing() noexcept = default;

    ByteRing(ByteRing&& other) noexcept
        : core_{std::move(other.core_)}
        , owns_storage_{std::exchange(other.owns_storage_, false)}
    {}

    ByteRing& operator=(ByteRing&& other) noexcept
    {
        if (this != &other) {
            clear();
            release_storage();
            core_         = std::move(other.core_);
            owns_storage_ = std::exchange(other.owns_storage_, false);
        }
        return *this;
    }

    ByteRing(const ByteRing&)            = delete;
    ByteRing& operator=(const ByteRing&) = delete;

    ~ByteRing() { clear(); release_storage(); }

    [[nodiscard]] status init(std::byte* storage, std::size_t capacity) noexcept
    {
        detail::typed_element_policy<std::uint8_t> policy{};
        return core_.init(
            policy,
            storage,
            capacity,
            detail::ring_policy::none
        );
    }

    [[nodiscard]] status init(stl::byte_span storage, std::size_t capacity) noexcept
    {
        if (storage.size() < capacity) {
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
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t capacity,
        byte_ring_policy policy = byte_ring_policy::none
    )
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(capacity, alignof(std::byte), &ptr);
        if (!ok(st)) {
            return st;
        }

        detail::typed_element_policy<std::uint8_t> ep{};
        return core_.init(
            ep,
            static_cast<std::byte*>(ptr),
            capacity,
            to_detail_policy(policy)
        );
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status push_byte(std::uint8_t byte)
    {
        return core_.push_back(&byte, has_overwrite_on_full(), false);
    }

    [[nodiscard]] status pop_byte(std::uint8_t* out)
    {
        return core_.pop_front(out != nullptr ? static_cast<void*>(out) : nullptr);
    }

    [[nodiscard]] status push_bytes(const std::uint8_t* data, std::size_t count, std::size_t* out_written = nullptr)
    {
        if (data == nullptr && count > 0u) {
            return status::null_ptr;
        }

        std::size_t written = 0u;
        while (written < count) {
            const status st = push_byte(data[written]);
            if (st == status::full) {
                break;
            }
            if (!ok(st)) {
                if (out_written != nullptr) {
                    *out_written = written;
                }
                return st;
            }
            ++written;
        }

        if (out_written != nullptr) {
            *out_written = written;
        }
        return written == count ? status::ok : status::full;
    }

    [[nodiscard]] status pop_bytes(std::uint8_t* out, std::size_t count, std::size_t* out_read = nullptr)
    {
        if (out == nullptr && count > 0u) {
            return status::null_ptr;
        }

        std::size_t read = 0u;
        while (read < count && !empty()) {
            const status st = pop_byte(out != nullptr ? out + read : nullptr);
            if (st == status::empty) {
                break;
            }
            if (!ok(st)) {
                if (out_read != nullptr) {
                    *out_read = read;
                }
                return st;
            }
            ++read;
        }

        if (out_read != nullptr) {
            *out_read = read;
        }
        return read == count ? status::ok : status::empty;
    }

    [[nodiscard]] std::size_t readable_contiguous(const std::uint8_t** out_ptr) const noexcept
    {
        const void* ptr = nullptr;
        const std::size_t n = core_.readable_contiguous(&ptr);
        if (out_ptr != nullptr) {
            *out_ptr = n > 0u ? static_cast<const std::uint8_t*>(ptr) : nullptr;
        }
        return n;
    }

    [[nodiscard]] std::size_t writable_contiguous(std::uint8_t** out_ptr) noexcept
    {
        void* ptr = nullptr;
        const std::size_t n = core_.writable_contiguous(&ptr);
        if (out_ptr != nullptr) {
            *out_ptr = n > 0u ? static_cast<std::uint8_t*>(ptr) : nullptr;
        }
        return n;
    }

    void commit_read(std::size_t byte_count) noexcept { core_.commit_read(byte_count); }
    void commit_write(std::size_t byte_count) noexcept { core_.commit_write(byte_count); }

private:
    [[nodiscard]] static detail::ring_policy to_detail_policy(byte_ring_policy policy) noexcept
    {
        if ((static_cast<unsigned>(policy) &
             static_cast<unsigned>(byte_ring_policy::overwrite_on_full)) != 0u) {
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
#if MEMKIT_ALLOW_HEAP
            std::free(core_.storage());
#endif
        }
        owns_storage_ = false;
    }

    detail::ring_core<detail::typed_element_policy<std::uint8_t>> core_{};
    bool                                                          owns_storage_ = false;
};

} // namespace memkit
