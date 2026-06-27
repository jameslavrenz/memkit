#include "memkit/memory/mmap.hpp"

#if MEMKIT_ALLOW_MMAP

#include <cstdlib>

#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace memkit::memory {

mmap_storage::mmap_storage(mmap_storage&& other) noexcept
    : base_{other.base_}
    , bytes_{other.bytes_}
{
    other.base_  = nullptr;
    other.bytes_ = 0u;
}

mmap_storage& mmap_storage::operator=(mmap_storage&& other) noexcept
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

mmap_storage::~mmap_storage()
{
    release();
}

mmap_storage mmap_storage::map(std::size_t bytes)
{
    mmap_storage storage;
    if (bytes == 0u) {
        return storage;
    }

#ifndef _WIN32
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return storage;
    }

    const std::size_t page = static_cast<std::size_t>(page_size);
    const std::size_t mapped_bytes =
        ((bytes + page - 1u) / page) * page;

    void* const ptr = mmap(
        nullptr,
        mapped_bytes,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    if (ptr == MAP_FAILED) {
        return storage;
    }

    storage.base_  = static_cast<std::byte*>(ptr);
    storage.bytes_ = mapped_bytes;
#endif

    return storage;
}

void mmap_storage::release() noexcept
{
    if (base_ == nullptr) {
        return;
    }

#ifndef _WIN32
    munmap(base_, bytes_);
#endif

    base_  = nullptr;
    bytes_ = 0u;
}

std::byte* mmap_storage::detach() noexcept
{
    std::byte* const ptr = base_;
    base_  = nullptr;
    bytes_ = 0u;
    return ptr;
}

} // namespace memkit::memory

#endif // MEMKIT_ALLOW_MMAP
