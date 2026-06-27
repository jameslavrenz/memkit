#pragma once

#include "../detail/sparse_set_core.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <utility>

namespace memkit {

/** O(1) insert/remove/contains/iterate set over dense integer IDs in [0, max_id). */
class SparseSet {
public:
    SparseSet() noexcept = default;

    SparseSet(SparseSet&& other) noexcept
        : core_{std::move(other.core_)}
    {
        other.core_.reset_state();
    }

    SparseSet& operator=(SparseSet&& other) noexcept
    {
        if (this != &other) {
            core_ = std::move(other.core_);
            other.core_.reset_state();
        }
        return *this;
    }

    SparseSet(const SparseSet&)            = delete;
    SparseSet& operator=(const SparseSet&) = delete;

    [[nodiscard]] static constexpr std::size_t dense_bytes(std::size_t max_id) noexcept
    {
        return detail::sparse_set_core::dense_bytes(max_id);
    }

    [[nodiscard]] static constexpr std::size_t sparse_bytes(std::size_t max_id) noexcept
    {
        return detail::sparse_set_core::sparse_bytes(max_id);
    }

    [[nodiscard]] status init(
        std::size_t* dense,
        std::size_t* sparse,
        std::size_t max_id
    ) noexcept
    {
        return core_.init(dense, sparse, max_id);
    }

    template<std::size_t MaxId>
    [[nodiscard]] status init(
        stl::array<std::size_t, MaxId>& dense,
        stl::array<std::size_t, MaxId>& sparse
    ) noexcept
    {
        return init(dense.data(), sparse.data(), MaxId);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t max_id)
    {
        if (max_id == 0u) {
            return status::invalid;
        }

        void* dense_ptr = nullptr;
        status st = arena.allocate(dense_bytes(max_id), alignof(std::size_t), &dense_ptr);
        if (!ok(st)) {
            return st;
        }

        void* sparse_ptr = nullptr;
        st = arena.allocate(sparse_bytes(max_id), alignof(std::size_t), &sparse_ptr);
        if (!ok(st)) {
            return st;
        }

        return init(
            static_cast<std::size_t*>(dense_ptr),
            static_cast<std::size_t*>(sparse_ptr),
            max_id
        );
    }

    [[nodiscard]] std::size_t max_id() const noexcept { return core_.max_id(); }
    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] bool contains(std::size_t id) const noexcept { return core_.contains(id); }

    [[nodiscard]] status insert(std::size_t id) noexcept { return core_.insert(id); }

    [[nodiscard]] status remove(std::size_t id) noexcept { return core_.remove(id); }

    [[nodiscard]] std::size_t operator[](std::size_t index) const noexcept { return core_.at(index); }

private:
    detail::sparse_set_core core_{};
};

} // namespace memkit
