#pragma once

#include "../status.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace memkit::detail {

template<typename T>
struct numeric_accumulator {
    using type = std::conditional_t<
        std::is_integral_v<T>,
        std::conditional_t<
            std::is_signed_v<T>,
            std::int64_t,
            std::uint64_t
        >,
        long double
    >;
};

} // namespace memkit::detail

namespace memkit {

/** Fixed-window moving average (sensor filtering, no heap). */
template<typename T, std::size_t N>
class MovingAverage {
public:
    static constexpr std::size_t window_size = N;

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0u; }
    [[nodiscard]] bool full() const noexcept { return count_ >= N; }

    void clear() noexcept
    {
        count_ = 0u;
        index_ = 0u;
        sum_   = acc_type{0};
    }

    [[nodiscard]] status push(T sample) noexcept
    {
        if (N == 0u) {
            return status::invalid;
        }

        if (count_ < N) {
            samples_[index_] = sample;
            sum_ += static_cast<acc_type>(sample);
            ++count_;
            index_ = (index_ + 1u) % N;
            return status::ok;
        }

        sum_ -= static_cast<acc_type>(samples_[index_]);
        samples_[index_] = sample;
        sum_ += static_cast<acc_type>(sample);
        index_ = (index_ + 1u) % N;
        return status::ok;
    }

    [[nodiscard]] T average() const noexcept
    {
        if (count_ == 0u) {
            return T{};
        }

        if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(sum_ / static_cast<acc_type>(count_));
        } else {
            return static_cast<T>(static_cast<long double>(sum_) /
                                  static_cast<long double>(count_));
        }
    }

    [[nodiscard]] long double average_precise() const noexcept
    {
        if (count_ == 0u) {
            return 0.0L;
        }
        return static_cast<long double>(sum_) / static_cast<long double>(count_);
    }

private:
    using acc_type = typename detail::numeric_accumulator<T>::type;

    T           samples_[N]{};
    std::size_t index_ = 0u;
    std::size_t count_ = 0u;
    acc_type    sum_   = acc_type{0};
};

/** Fixed-window min/max/average stats (small N, no heap). */
template<typename T, std::size_t N>
class WindowStats {
public:
    static constexpr std::size_t window_size = N;

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0u; }
    [[nodiscard]] bool full() const noexcept { return count_ >= N; }

    void clear() noexcept
    {
        count_ = 0u;
        index_ = 0u;
    }

    [[nodiscard]] status push(T sample) noexcept
    {
        if (N == 0u) {
            return status::invalid;
        }

        if (count_ < N) {
            samples_[index_] = sample;
            ++count_;
            index_ = (index_ + 1u) % N;
            return status::ok;
        }

        samples_[index_] = sample;
        index_ = (index_ + 1u) % N;
        return status::ok;
    }

    [[nodiscard]] T min() const noexcept
    {
        if (count_ == 0u) {
            return T{};
        }

        T value = samples_[0];
        for (std::size_t i = 1u; i < count_; ++i) {
            if (samples_[i] < value) {
                value = samples_[i];
            }
        }
        return value;
    }

    [[nodiscard]] T max() const noexcept
    {
        if (count_ == 0u) {
            return T{};
        }

        T value = samples_[0];
        for (std::size_t i = 1u; i < count_; ++i) {
            if (samples_[i] > value) {
                value = samples_[i];
            }
        }
        return value;
    }

    [[nodiscard]] T average() const noexcept
    {
        if (count_ == 0u) {
            return T{};
        }

        using acc_type = typename detail::numeric_accumulator<T>::type;
        acc_type sum   = acc_type{0};
        for (std::size_t i = 0u; i < count_; ++i) {
            sum += static_cast<acc_type>(samples_[i]);
        }

        if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(sum / static_cast<acc_type>(count_));
        } else {
            return static_cast<T>(static_cast<long double>(sum) /
                                  static_cast<long double>(count_));
        }
    }

private:
    T           samples_[N]{};
    std::size_t index_ = 0u;
    std::size_t count_ = 0u;
};

} // namespace memkit
