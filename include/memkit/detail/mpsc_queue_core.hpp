#pragma once

#include "../status.hpp"
#include "element_policy.hpp"
#include "utility.hpp"

#include <atomic>
#include <cstddef>
#include <cstring>
#include <new>

namespace memkit::detail {

enum class mpsc_policy : std::uint8_t {
    none = 0u,
};

template<typename Policy>
class mpsc_queue_core {
public:
    mpsc_queue_core() = default;

    [[nodiscard]] static std::size_t cell_stride(const Policy& policy) noexcept
    {
        return align_up(
            sizeof(std::atomic<std::size_t>) + policy.elem_size(),
            alignof(std::atomic<std::size_t>)
        );
    }

    [[nodiscard]] static std::size_t storage_bytes(
        const Policy& policy,
        std::size_t capacity_pow2
    ) noexcept
    {
        return cell_stride(policy) * capacity_pow2;
    }

    [[nodiscard]] status init(
        Policy policy,
        std::byte* cell_storage,
        std::size_t capacity_pow2,
        mpsc_policy queue_policy = mpsc_policy::none
    ) noexcept
    {
        if (cell_storage == nullptr || capacity_pow2 < 2u || !is_power_of_two(capacity_pow2)) {
            return status::invalid;
        }

        destroy_cells();

        policy_       = policy;
        cells_        = cell_storage;
        capacity_     = capacity_pow2;
        mask_         = capacity_pow2 - 1u;
        queue_policy_ = queue_policy;
        cell_stride_  = cell_stride(policy);
        enqueue_pos_.store(0u, std::memory_order_relaxed);
        dequeue_pos_  = 0u;

        for (std::size_t i = 0u; i < capacity_; ++i) {
            new (cells_ + (i * cell_stride_)) std::atomic<std::size_t>(i);
        }

        return status::ok;
    }

    void reset_state() noexcept
    {
        destroy_cells();
        policy_      = Policy{};
        cells_       = nullptr;
        capacity_    = 0u;
        mask_        = 0u;
        cell_stride_ = 0u;
        enqueue_pos_.store(0u, std::memory_order_relaxed);
        dequeue_pos_ = 0u;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] std::size_t size() const noexcept
    {
        const std::size_t tail = enqueue_pos_.load(std::memory_order_acquire);
        return tail - dequeue_pos_;
    }

    [[nodiscard]] bool empty() const noexcept { return size() == 0u; }

    [[nodiscard]] bool full() const noexcept { return size() >= capacity_; }

    [[nodiscard]] status push(const void* elem, std::size_t max_spins = 1024u) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }

        const std::size_t pos = enqueue_pos_.fetch_add(1u, std::memory_order_relaxed);
        std::atomic<std::size_t>& seq = cell_seq(pos & mask_);

        for (std::size_t spin = 0u; spin < max_spins; ++spin) {
            const std::size_t sequence = seq.load(std::memory_order_acquire);
            if (sequence == pos) {
                policy_.copy_construct(cell_data(pos & mask_), elem);
                seq.store(pos + 1u, std::memory_order_release);
                return status::ok;
            }
        }

        return status::full;
    }

    [[nodiscard]] status pop(void* out, bool move_out = true, std::size_t max_spins = 1024u) noexcept
    {
        const std::size_t pos = dequeue_pos_;
        std::atomic<std::size_t>& seq = cell_seq(pos & mask_);

        for (std::size_t spin = 0u; spin < max_spins; ++spin) {
            const std::size_t sequence = seq.load(std::memory_order_acquire);
            if (sequence == pos + 1u) {
                void* src = cell_data(pos & mask_);
                if (out != nullptr) {
                    if (move_out) {
                        policy_.move_construct(out, src);
                        policy_.destroy(src);
                    } else {
                        policy_.copy_construct(out, src);
                        policy_.destroy(src);
                    }
                } else {
                    policy_.destroy(src);
                }

                seq.store(pos + capacity_, std::memory_order_release);
                dequeue_pos_ = pos + 1u;
                return status::ok;
            }

            if (sequence == pos &&
                enqueue_pos_.load(std::memory_order_acquire) == pos) {
                return status::empty;
            }
        }

        return status::full;
    }

    void clear() noexcept
    {
        while (pop(nullptr, true) == status::ok) {
        }
    }

    void destroy_cells() noexcept
    {
        if (cells_ == nullptr || capacity_ == 0u) {
            return;
        }

        for (std::size_t i = 0u; i < capacity_; ++i) {
            cell_seq(i).~atomic();
        }
    }

private:
    [[nodiscard]] std::atomic<std::size_t>& cell_seq(std::size_t index) const noexcept
    {
        return *reinterpret_cast<std::atomic<std::size_t>*>(cells_ + (index * cell_stride_));
    }

    [[nodiscard]] void* cell_data(std::size_t index) const noexcept
    {
        return cells_ + (index * cell_stride_) + sizeof(std::atomic<std::size_t>);
    }

    Policy                   policy_{};
    std::byte*               cells_       = nullptr;
    std::size_t              capacity_    = 0u;
    std::size_t              mask_        = 0u;
    std::size_t              cell_stride_ = 0u;
    mpsc_policy              queue_policy_ = mpsc_policy::none;
    std::atomic<std::size_t> enqueue_pos_{0u};
    std::size_t              dequeue_pos_ = 0u;
};

} // namespace memkit::detail
