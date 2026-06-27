#pragma once

#include "../detail/element_policy.hpp"
#include "../detail/ring_core.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <utility>

namespace memkit {

/** Circular flight-recorder log; always overwrites oldest entry when full. */
template<typename Record>
class RingLog {
public:
    RingLog() noexcept = default;

    RingLog(RingLog&& other) noexcept
        : core_{std::move(other.core_)}
    {
        other.core_.reset_state();
    }

    RingLog& operator=(RingLog&& other) noexcept
    {
        if (this != &other) {
            clear();
            core_ = std::move(other.core_);
            other.core_.reset_state();
        }
        return *this;
    }

    RingLog(const RingLog&)            = delete;
    RingLog& operator=(const RingLog&) = delete;

    ~RingLog() { clear(); }

    [[nodiscard]] static constexpr std::size_t storage_bytes(std::size_t capacity) noexcept
    {
        return capacity * sizeof(Record);
    }

    template<std::size_t Capacity>
    [[nodiscard]] static constexpr std::size_t storage_bytes() noexcept
    {
        return storage_bytes(Capacity);
    }

    [[nodiscard]] status init(std::byte* storage, std::size_t capacity) noexcept
    {
        if (storage == nullptr || capacity == 0u) {
            return status::invalid;
        }

        detail::typed_element_policy<Record> policy{};
        return core_.init(
            policy,
            storage,
            capacity,
            detail::ring_policy::overwrite_on_full
        );
    }

    template<std::size_t Capacity>
    [[nodiscard]] status init(stl::array<std::byte, storage_bytes<Capacity>()>& storage) noexcept
    {
        return init(storage.data(), Capacity);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t capacity)
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(storage_bytes(capacity), alignof(Record), &ptr);
        if (!ok(st)) {
            return st;
        }

        return init(static_cast<std::byte*>(ptr), capacity);
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status append(const Record& record)
    {
        return core_.push_back(&record, true, false);
    }

    [[nodiscard]] status append(Record&& record)
    {
        return core_.push_back(&record, true, true);
    }

    [[nodiscard]] status peek_oldest(Record& out) const
    {
        return core_.peek_front(static_cast<void*>(&out));
    }

    [[nodiscard]] status peek_newest(Record& out) const
    {
        return core_.peek_back(static_cast<void*>(&out));
    }

    template<typename Visitor>
    [[nodiscard]] status foreach_chronological(Visitor&& visit) const
    {
        return core_.foreach([&visit](const void* slot, std::size_t index) {
            (void)index;
            return visit(*static_cast<const Record*>(slot));
        });
    }

    template<typename Visitor>
    [[nodiscard]] status foreach_newest_first(Visitor&& visit) const
    {
        const std::size_t count = core_.size();
        for (std::size_t i = count; i > 0u; --i) {
            const auto* record = static_cast<const Record*>(core_.logical_slot(i - 1u));
            const status st    = visit(*record);
            if (!ok(st)) {
                return st;
            }
        }
        return status::ok;
    }

private:
    detail::ring_core<detail::typed_element_policy<Record>> core_{};
};

} // namespace memkit
