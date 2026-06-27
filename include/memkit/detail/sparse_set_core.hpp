#pragma once

#include "../status.hpp"

#include <cstddef>

namespace memkit::detail {

class sparse_set_core {
public:
    sparse_set_core() = default;

    [[nodiscard]] static constexpr std::size_t dense_bytes(std::size_t max_id) noexcept
    {
        return max_id * sizeof(std::size_t);
    }

    [[nodiscard]] static constexpr std::size_t sparse_bytes(std::size_t max_id) noexcept
    {
        return max_id * sizeof(std::size_t);
    }

    [[nodiscard]] status init(
        std::size_t* dense,
        std::size_t* sparse,
        std::size_t max_id
    ) noexcept
    {
        if (dense == nullptr || sparse == nullptr || max_id == 0u) {
            return status::invalid;
        }

        dense_  = dense;
        sparse_ = sparse;
        max_id_ = max_id;
        size_   = 0u;
        return status::ok;
    }

    void reset_state() noexcept
    {
        dense_  = nullptr;
        sparse_ = nullptr;
        max_id_ = 0u;
        size_   = 0u;
    }

    [[nodiscard]] std::size_t max_id() const noexcept { return max_id_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }

    [[nodiscard]] bool contains(std::size_t id) const noexcept
    {
        if (id >= max_id_) {
            return false;
        }

        const std::size_t pos = sparse_[id];
        return pos < size_ && dense_[pos] == id;
    }

    [[nodiscard]] status insert(std::size_t id) noexcept
    {
        if (id >= max_id_) {
            return status::invalid;
        }
        if (contains(id)) {
            return status::ok;
        }

        dense_[size_] = id;
        sparse_[id]   = size_;
        ++size_;
        return status::ok;
    }

    [[nodiscard]] status remove(std::size_t id) noexcept
    {
        if (!contains(id)) {
            return status::not_found;
        }

        const std::size_t pos     = sparse_[id];
        const std::size_t last_id = dense_[size_ - 1u];

        dense_[pos]     = last_id;
        sparse_[last_id] = pos;
        sparse_[id]     = 0u;
        --size_;
        return status::ok;
    }

    [[nodiscard]] std::size_t at(std::size_t index) const noexcept
    {
        return dense_[index];
    }

    void clear() noexcept { size_ = 0u; }

private:
    std::size_t* dense_  = nullptr;
    std::size_t* sparse_ = nullptr;
    std::size_t  max_id_ = 0u;
    std::size_t  size_   = 0u;
};

} // namespace memkit::detail
