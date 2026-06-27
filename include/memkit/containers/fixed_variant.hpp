#pragma once

#include "../status.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace memkit::detail {

template<typename T, typename First, typename... Rest>
struct fixed_variant_index_impl {
    static constexpr std::size_t value =
        std::is_same_v<T, First> ? 0u : (1u + fixed_variant_index_impl<T, Rest...>::value);
};

template<typename T, typename Last>
struct fixed_variant_index_impl<T, Last> {
    static constexpr std::size_t value = std::is_same_v<T, Last> ? 0u : 1u;
};

template<typename T, typename... Ts>
struct fixed_variant_index {
    static_assert((std::is_same_v<T, Ts> || ...), "Type is not a member of the variant");
    static constexpr std::size_t value = fixed_variant_index_impl<T, Ts...>::value;
};

template<typename... Ts>
struct fixed_variant_traits {
    static constexpr std::size_t type_count    = sizeof...(Ts);
    static constexpr std::size_t invalid_index = type_count;
    static constexpr std::size_t max_size      = std::max({sizeof(Ts)...});
    static constexpr std::size_t max_align     = std::max({alignof(Ts)...});
};

template<std::size_t I, typename... Ts>
struct fixed_variant_alternative;

template<typename First, typename... Rest>
struct fixed_variant_alternative<0u, First, Rest...> {
    using type = First;
};

template<std::size_t I, typename First, typename... Rest>
struct fixed_variant_alternative<I, First, Rest...> {
    using type = typename fixed_variant_alternative<I - 1u, Rest...>::type;
};

} // namespace memkit::detail

namespace memkit {

/** Fixed-storage tagged union over a closed set of types (no heap). */
template<typename... Ts>
class FixedVariant {
public:
    static constexpr std::size_t type_count = sizeof...(Ts);

    FixedVariant() noexcept = default;

    FixedVariant(FixedVariant&& other) noexcept { move_from(std::move(other)); }

    FixedVariant& operator=(FixedVariant&& other) noexcept
    {
        if (this != &other) {
            clear();
            move_from(std::move(other));
        }
        return *this;
    }

    FixedVariant(const FixedVariant&)            = delete;
    FixedVariant& operator=(const FixedVariant&) = delete;

    ~FixedVariant() { clear(); }

    [[nodiscard]] bool empty() const noexcept { return index_ == traits::invalid_index; }

    [[nodiscard]] std::size_t index() const noexcept { return index_; }

    template<typename T>
    [[nodiscard]] bool holds() const noexcept
    {
        return !empty() && index_ == detail::fixed_variant_index<T, Ts...>::value;
    }

    template<typename T, typename... Args>
    [[nodiscard]] status emplace(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>
    )
    {
        static_assert((std::is_same_v<T, Ts> || ...), "Type is not a member of the variant");

        clear();
        new (storage_) T(std::forward<Args>(args)...);
        index_ = detail::fixed_variant_index<T, Ts...>::value;
        return status::ok;
    }

    template<typename T>
    [[nodiscard]] status set(T&& value) noexcept(std::is_nothrow_constructible_v<T, T&&>)
    {
        return emplace<T>(std::forward<T>(value));
    }

    template<typename T>
    [[nodiscard]] status get(T& out) const noexcept
    {
        if (!holds<T>()) {
            return status::invalid;
        }

        out = *std::launder(reinterpret_cast<const T*>(storage_));
        return status::ok;
    }

    template<typename T>
    [[nodiscard]] T* try_get() noexcept
    {
        if (!holds<T>()) {
            return nullptr;
        }
        return std::launder(reinterpret_cast<T*>(storage_));
    }

    template<typename T>
    [[nodiscard]] const T* try_get() const noexcept
    {
        if (!holds<T>()) {
            return nullptr;
        }
        return std::launder(reinterpret_cast<const T*>(storage_));
    }

    template<typename Visitor>
    [[nodiscard]] status visit(Visitor&& visitor) const
    {
        if (empty()) {
            return status::empty;
        }
        return visit_at(index_, std::forward<Visitor>(visitor));
    }

    void clear() noexcept
    {
        if (empty()) {
            return;
        }
        destroy_at(index_);
        index_ = traits::invalid_index;
    }

private:
    using traits = detail::fixed_variant_traits<Ts...>;

    alignas(traits::max_align) std::byte storage_[traits::max_size]{};
    std::size_t index_ = traits::invalid_index;

    void move_from(FixedVariant&& other) noexcept
    {
        if (other.empty()) {
            index_ = traits::invalid_index;
            return;
        }

        move_at(other.index_, other.storage_, storage_);
        index_       = other.index_;
        other.index_ = traits::invalid_index;
    }

    template<typename Visitor, std::size_t I = 0u>
    [[nodiscard]] status visit_at(std::size_t index, Visitor&& visitor) const
    {
        if constexpr (I >= type_count) {
            (void)index;
            (void)visitor;
            return status::invalid;
        } else if (I == index) {
            using alt = typename detail::fixed_variant_alternative<I, Ts...>::type;
            return visitor(*std::launder(reinterpret_cast<const alt*>(storage_)));
        } else {
            return visit_at<Visitor, I + 1u>(index, std::forward<Visitor>(visitor));
        }
    }

    template<std::size_t I = 0u>
    void destroy_at(std::size_t index) noexcept
    {
        if constexpr (I >= type_count) {
            (void)index;
            return;
        } else if (I == index) {
            using alt = typename detail::fixed_variant_alternative<I, Ts...>::type;
            std::launder(reinterpret_cast<alt*>(storage_))->~alt();
        } else {
            destroy_at<I + 1u>(index);
        }
    }

    template<std::size_t I = 0u>
    void move_at(std::size_t index, std::byte* src, std::byte* dst) noexcept
    {
        if constexpr (I >= type_count) {
            (void)index;
            (void)src;
            (void)dst;
            return;
        } else if (I == index) {
            using alt = typename detail::fixed_variant_alternative<I, Ts...>::type;
            new (dst) alt(std::move(*std::launder(reinterpret_cast<alt*>(src))));
            std::launder(reinterpret_cast<alt*>(src))->~alt();
        } else {
            move_at<I + 1u>(index, src, dst);
        }
    }
};

} // namespace memkit
