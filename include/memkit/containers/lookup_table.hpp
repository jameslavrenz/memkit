#pragma once

#include "../status.hpp"

#include <cstddef>
#include <type_traits>

namespace memkit {

enum class lookup_mode : std::uint8_t {
    interpolate = 0,
    nearest     = 1,
    exact       = 2,
};

/** Sorted-key lookup table with piecewise-linear interpolation (sensor calibration). */
template<typename X, typename Y>
class LookupTable {
public:
    LookupTable() noexcept = default;

    [[nodiscard]] status init(
        const X* keys,
        const Y* values,
        std::size_t count,
        lookup_mode mode = lookup_mode::interpolate
    ) noexcept
    {
        if (keys == nullptr || values == nullptr || count == 0u) {
            return status::invalid;
        }

        keys_   = keys;
        values_ = values;
        count_  = count;
        mode_   = mode;
        return status::ok;
    }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] lookup_mode mode() const noexcept { return mode_; }

    [[nodiscard]] Y at(X x) const noexcept
    {
        if (count_ == 0u) {
            return Y{};
        }
        if (count_ == 1u) {
            return values_[0];
        }

        if (x <= keys_[0]) {
            return values_[0];
        }
        if (x >= keys_[count_ - 1u]) {
            return values_[count_ - 1u];
        }

        std::size_t lo = 0u;
        std::size_t hi = count_ - 1u;
        while (lo + 1u < hi) {
            const std::size_t mid = lo + ((hi - lo) >> 1u);
            if (keys_[mid] <= x) {
                lo = mid;
            } else {
                hi = mid;
            }
        }

        if (mode_ == lookup_mode::nearest) {
            const X dx_lo = x - keys_[lo];
            const X dx_hi = keys_[hi] - x;
            return dx_hi < dx_lo ? values_[hi] : values_[lo];
        }

        if (mode_ == lookup_mode::exact) {
            if (keys_[lo] == x) {
                return values_[lo];
            }
            if (keys_[hi] == x) {
                return values_[hi];
            }
            return Y{};
        }

        return interpolate(keys_[lo], values_[lo], keys_[hi], values_[hi], x);
    }

    [[nodiscard]] status lookup(X key, Y& out) const noexcept
    {
        if (count_ == 0u) {
            return status::invalid;
        }

        if (mode_ == lookup_mode::exact) {
            for (std::size_t i = 0u; i < count_; ++i) {
                if (keys_[i] == key) {
                    out = values_[i];
                    return status::ok;
                }
            }
            return status::not_found;
        }

        out = at(key);
        return status::ok;
    }

private:
    [[nodiscard]] static Y interpolate(X x0, Y y0, X x1, Y y1, X x) noexcept
    {
        if constexpr (std::is_floating_point_v<X>) {
            if (x1 == x0) {
                return y0;
            }
            const long double t = static_cast<long double>(x - x0) /
                                  static_cast<long double>(x1 - x0);
            if constexpr (std::is_floating_point_v<Y>) {
                return static_cast<Y>(
                    static_cast<long double>(y0) +
                    t * (static_cast<long double>(y1) - static_cast<long double>(y0))
                );
            } else {
                return static_cast<Y>(static_cast<long double>(y0) +
                                        t * static_cast<long double>(y1 - y0));
            }
        } else {
            if (x1 == x0) {
                return y0;
            }
            const auto numer = static_cast<decltype(x - x0)>(x - x0);
            const auto denom = static_cast<decltype(x1 - x0)>(x1 - x0);
            if constexpr (std::is_integral_v<Y>) {
                const auto delta = static_cast<Y>(y1 - y0);
                return static_cast<Y>(y0 + (delta * static_cast<Y>(numer)) / static_cast<Y>(denom));
            } else {
                const long double t = static_cast<long double>(numer) /
                                      static_cast<long double>(denom);
                return static_cast<Y>(
                    static_cast<long double>(y0) +
                    t * (static_cast<long double>(y1) - static_cast<long double>(y0))
                );
            }
        }
    }

    const X*      keys_   = nullptr;
    const Y*      values_ = nullptr;
    std::size_t   count_  = 0u;
    lookup_mode   mode_   = lookup_mode::interpolate;
};

} // namespace memkit
