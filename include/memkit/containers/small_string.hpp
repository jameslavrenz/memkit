#pragma once

#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <cstring>

namespace memkit {

/** Fixed-capacity null-terminated string for MCU-friendly use (no heap). */
template<std::size_t N>
class SmallString {
public:
    static constexpr std::size_t capacity = N;

    SmallString() noexcept { clear(); }

    explicit SmallString(stl::string_view text) noexcept { (void)assign(text); }

    [[nodiscard]] std::size_t size() const noexcept { return len_; }
    [[nodiscard]] std::size_t length() const noexcept { return len_; }
    [[nodiscard]] bool empty() const noexcept { return len_ == 0u; }
    [[nodiscard]] bool full() const noexcept { return len_ >= N; }

    [[nodiscard]] const char* c_str() const noexcept { return buf_; }
    [[nodiscard]] stl::string_view view() const noexcept { return stl::string_view{buf_, len_}; }

    void clear() noexcept
    {
        len_     = 0u;
        buf_[0]  = '\0';
    }

    [[nodiscard]] status assign(stl::string_view text) noexcept
    {
        if (text.size() > N) {
            return status::invalid;
        }

        if (text.empty()) {
            clear();
            return status::ok;
        }

        std::memcpy(buf_, text.data(), text.size());
        len_           = text.size();
        buf_[len_]     = '\0';
        return status::ok;
    }

    [[nodiscard]] status assign(const char* text) noexcept
    {
        if (text == nullptr) {
            return status::null_ptr;
        }
        return assign(stl::string_view{text});
    }

    [[nodiscard]] status append(stl::string_view text) noexcept
    {
        if (text.size() + len_ > N) {
            return status::full;
        }
        if (text.empty()) {
            return status::ok;
        }

        std::memcpy(buf_ + len_, text.data(), text.size());
        len_ += text.size();
        buf_[len_] = '\0';
        return status::ok;
    }

    [[nodiscard]] status append(char ch) noexcept
    {
        if (len_ >= N) {
            return status::full;
        }

        buf_[len_]     = ch;
        ++len_;
        buf_[len_]     = '\0';
        return status::ok;
    }

    [[nodiscard]] status push_back(char ch) noexcept { return append(ch); }

    [[nodiscard]] bool operator==(stl::string_view other) const noexcept
    {
        return view() == other;
    }

    [[nodiscard]] bool operator!=(stl::string_view other) const noexcept
    {
        return !(*this == other);
    }

private:
    char        buf_[N + 1u]{};
    std::size_t len_ = 0u;
};

} // namespace memkit
