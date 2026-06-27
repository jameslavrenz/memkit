#pragma once

#include "../config.hpp"

#include <cstddef>
#include <cstdlib>

namespace memkit::memory {

#if MEMKIT_ALLOW_HEAP

/*
 * Owning heap-backed byte storage (MPU only).
 */
class heap_storage {
public:
    heap_storage() noexcept = default;

    heap_storage(const heap_storage&)            = delete;
    heap_storage& operator=(const heap_storage&) = delete;

    heap_storage(heap_storage&& other) noexcept
        : base_{other.base_}
        , bytes_{other.bytes_}
    {
        other.base_  = nullptr;
        other.bytes_ = 0u;
    }

    heap_storage& operator=(heap_storage&& other) noexcept
    {
        if (this != &other) {
            release();
            base_        = other.base_;
            bytes_       = other.bytes_;
            other.base_  = nullptr;
            other.bytes_ = 0u;
        }
        return *this;
    }

    ~heap_storage() { release(); }

    [[nodiscard]] static heap_storage allocate(std::size_t bytes)
    {
        heap_storage storage;
        if (bytes > 0u) {
            storage.base_ = static_cast<std::byte*>(std::malloc(bytes));
            if (storage.base_ != nullptr) {
                storage.bytes_ = bytes;
            }
        }
        return storage;
    }

    [[nodiscard]] std::byte*       data()       noexcept { return base_; }
    [[nodiscard]] const std::byte* data() const noexcept { return base_; }
    [[nodiscard]] std::size_t        size() const noexcept { return bytes_; }
    [[nodiscard]] bool               valid() const noexcept { return base_ != nullptr; }

    void release() noexcept
    {
        if (base_ != nullptr) {
            std::free(base_);
            base_  = nullptr;
            bytes_ = 0u;
        }
    }

private:
    std::byte*  base_  = nullptr;
    std::size_t bytes_ = 0u;
};

#endif // MEMKIT_ALLOW_HEAP

} // namespace memkit::memory
