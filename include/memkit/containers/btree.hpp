#pragma once

#include "../detail/btree_map_core.hpp"
#include "../detail/compare_policy.hpp"
#include "../detail/element_policy.hpp"
#include "../status.hpp"

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace memkit {

enum class btree_traversal : std::uint8_t {
    inorder   = 0,
    preorder  = 1,
    postorder = 2,
};

enum class btree_policy : unsigned {
    none           = 0u,
    fixed_capacity = 1u << 0u,
};

template<typename K, typename V, typename Compare = detail::type_compare<K>>
class BTree {
public:
    struct elem {
        K key{};
        V value{};
    };

    BTree() noexcept = default;

    BTree(BTree&& other) noexcept
        : core_{std::move(other.core_)}
        , compare_{std::move(other.compare_)}
    {
        other.core_.reset_state();
        rebuild_policies();
        other.rebuild_policies();
    }

    BTree& operator=(BTree&& other) noexcept
    {
        if (this != &other) {
            clear();
            core_    = std::move(other.core_);
            compare_ = std::move(other.compare_);
            other.core_.reset_state();
            rebuild_policies();
            other.rebuild_policies();
        }
        return *this;
    }

    BTree(const BTree&)            = delete;
    BTree& operator=(const BTree&) = delete;

    ~BTree() { clear(); }

    [[nodiscard]] static std::size_t node_stride() noexcept
    {
        return detail::btree_map_core<
            detail::runtime_element_policy,
            detail::runtime_compare_policy>::node_stride(sizeof(elem));
    }

    [[nodiscard]] static std::size_t pool_bytes(std::size_t capacity) noexcept
    {
        return detail::btree_map_core<
            detail::runtime_element_policy,
            detail::runtime_compare_policy>::pool_bytes(capacity, sizeof(elem));
    }

    [[nodiscard]] status init(std::byte* node_pool, std::size_t capacity) noexcept
    {
        if (node_pool == nullptr || capacity == 0u) {
            return status::invalid;
        }

        clear();
        rebuild_policies();

        return core_.init(
            elem_policy_,
            compare_policy_,
            node_pool,
            capacity,
            detail::btree_map_policy::fixed_pool
        );
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t capacity,
        btree_policy policy = btree_policy::none
    )
    {
        clear();
        rebuild_policies();

        if ((static_cast<unsigned>(policy) &
             static_cast<unsigned>(btree_policy::fixed_capacity)) != 0u) {
            if (capacity == 0u) {
                return status::invalid;
            }

            void* ptr = nullptr;
            const status st = arena.allocate(pool_bytes(capacity), alignof(std::max_align_t), &ptr);
            if (!ok(st)) {
                return st;
            }

            return init(static_cast<std::byte*>(ptr), capacity);
        }

        return core_.init_from_arena(arena, elem_policy_, compare_policy_);
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status insert(const K& key, const V& value) { return insert_impl(key, value); }
    [[nodiscard]] status insert(K&& key, const V& value) { return insert_impl(std::move(key), value); }
    [[nodiscard]] status insert(const K& key, V&& value) { return insert_impl(key, std::move(value)); }
    [[nodiscard]] status insert(K&& key, V&& value) { return insert_impl(std::move(key), std::move(value)); }

    [[nodiscard]] status get(const K& key, V& out) const
    {
        elem storage{};
        const status st = core_.get(&key, &storage);
        if (!ok(st)) {
            return st;
        }
        out = storage.value;
        return status::ok;
    }

    [[nodiscard]] bool contains(const K& key) const { return core_.contains(&key); }

    [[nodiscard]] status remove(const K& key) { return core_.remove(&key); }

    [[nodiscard]] status peek_min(K& out_key, V& out_value) const
    {
        elem out{};
        const status st = core_.peek_min(&out);
        if (!ok(st)) {
            return st;
        }
        out_key   = out.key;
        out_value = out.value;
        return status::ok;
    }

    [[nodiscard]] status peek_max(K& out_key, V& out_value) const
    {
        elem out{};
        const status st = core_.peek_max(&out);
        if (!ok(st)) {
            return st;
        }
        out_key   = out.key;
        out_value = out.value;
        return status::ok;
    }

    template<typename Visitor>
    [[nodiscard]] status foreach(btree_traversal order, Visitor&& visit) const
    {
        const detail::btree_traversal detail_order = [&order]() {
            switch (order) {
            case btree_traversal::preorder:
                return detail::btree_traversal::preorder;
            case btree_traversal::postorder:
                return detail::btree_traversal::postorder;
            default:
                return detail::btree_traversal::inorder;
            }
        }();

        return core_.foreach(detail_order, [&visit](const void* elem_ptr) {
            const auto* storage = static_cast<const elem*>(elem_ptr);
            return visit(storage->key, storage->value);
        });
    }

private:
    using map_core = detail::btree_map_core<
        detail::runtime_element_policy,
        detail::runtime_compare_policy>;

    template<typename KeyArg, typename ValueArg>
    [[nodiscard]] status insert_impl(KeyArg&& key, ValueArg&& value)
    {
        const elem storage{K(std::forward<KeyArg>(key)), V(std::forward<ValueArg>(value))};
        return core_.insert(&storage);
    }

    void rebuild_policies() noexcept
    {
        elem_policy_ = detail::runtime_element_policy{
            sizeof(elem),
            &BTree::elem_copy_trampoline,
            &BTree::elem_destroy_trampoline,
            this,
        };
        compare_policy_ = detail::runtime_compare_policy{
            sizeof(K),
            &BTree::compare_trampoline,
            this,
        };
    }

    static status elem_copy_trampoline(void* dst, const void* src, void* user)
    {
        static_cast<BTree*>(user)->elem_typed_.copy_construct(dst, src);
        return status::ok;
    }

    static void elem_destroy_trampoline(void* elem_ptr, void* user)
    {
        static_cast<BTree*>(user)->elem_typed_.destroy(elem_ptr);
    }

    static int compare_trampoline(const void* a, const void* b, void* user)
    {
        auto* self = static_cast<BTree*>(user);
        return self->compare_(*static_cast<const K*>(a), *static_cast<const K*>(b));
    }

    map_core                           core_{};
    [[no_unique_address]] Compare      compare_{};
    detail::typed_element_policy<elem>   elem_typed_{};
    detail::runtime_element_policy       elem_policy_{};
    detail::runtime_compare_policy       compare_policy_{};
};

} // namespace memkit
