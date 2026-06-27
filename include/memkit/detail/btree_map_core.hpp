#pragma once

#include "../status.hpp"
#include "compare_policy.hpp"
#include "element_policy.hpp"
#include "utility.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

namespace memkit::detail {

enum class btree_traversal : std::uint8_t {
    inorder   = 0,
    preorder  = 1,
    postorder = 2,
};

enum class btree_map_policy : std::uint8_t {
    none         = 0u,
    fixed_pool   = 1u << 0u,
    heap_dynamic = 1u << 1u,
};

enum class btree_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline btree_storage_kind operator|(
    btree_storage_kind a,
    btree_storage_kind b
) noexcept
{
    return static_cast<btree_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

struct btree_node_header {
    btree_node_header* left  = nullptr;
    btree_node_header* right = nullptr;
};

template<typename ElementPolicy, typename ComparePolicy>
class btree_map_core {
public:
    btree_map_core() = default;

    [[nodiscard]] static std::size_t node_stride(std::size_t elem_size) noexcept
    {
        const std::size_t header = align_up(sizeof(btree_node_header), alignof(btree_node_header));
        return header + elem_size;
    }

    [[nodiscard]] static std::size_t pool_bytes(std::size_t node_capacity, std::size_t elem_size) noexcept
    {
        return node_stride(elem_size) * node_capacity;
    }

    [[nodiscard]] status init(
        ElementPolicy policy,
        ComparePolicy compare,
        std::byte* node_pool,
        std::size_t node_capacity,
        btree_map_policy map_flags = btree_map_policy::fixed_pool
    ) noexcept
    {
        if (node_pool == nullptr || node_capacity == 0u) {
            return status::invalid;
        }

        policy_        = policy;
        compare_       = std::move(compare);
        node_pool_     = node_pool;
        node_capacity_ = node_capacity;
        root_          = nullptr;
        size_          = 0u;
        map_flags_     = map_flags;
        storage_kind_  = btree_storage_kind::external;
        return init_free_list();
    }

    [[nodiscard]] status init_dynamic(ElementPolicy policy, ComparePolicy compare) noexcept
    {
        policy_        = policy;
        compare_       = std::move(compare);
        node_pool_     = nullptr;
        node_capacity_ = 0u;
        root_          = nullptr;
        free_list_     = nullptr;
        size_          = 0u;
        map_flags_     = btree_map_policy::heap_dynamic;
        storage_kind_  = btree_storage_kind::external;
        return status::ok;
    }

    using raw_alloc_fn = status (*)(void* ctx, std::size_t bytes, std::size_t alignment, void** out_ptr);

    void bind_allocator(void* ctx, raw_alloc_fn fn) noexcept
    {
        arena_ctx_      = ctx;
        arena_alloc_fn_ = fn;
    }

    void unbind_allocator() noexcept
    {
        arena_ctx_      = nullptr;
        arena_alloc_fn_ = nullptr;
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, ElementPolicy policy, ComparePolicy compare) noexcept
    {
        const status st = init_dynamic(policy, compare);
        if (!ok(st)) {
            return st;
        }
        bind_arena(arena);
        storage_kind_ = btree_storage_kind::arena;
        return status::ok;
    }

    void reset_state() noexcept
    {
        policy_        = ElementPolicy{};
        compare_       = ComparePolicy{};
        root_          = nullptr;
        free_list_     = nullptr;
        size_          = 0u;
        node_capacity_ = 0u;
        node_pool_     = nullptr;
        map_flags_     = btree_map_policy::none;
        storage_kind_  = btree_storage_kind::external;
        unbind_allocator();
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept
    {
        if ((static_cast<unsigned>(map_flags_) &
             static_cast<unsigned>(btree_map_policy::fixed_pool)) != 0u) {
            return node_capacity_;
        }
        return SIZE_MAX;
    }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }
    [[nodiscard]] btree_map_policy flags() const noexcept { return map_flags_; }
    [[nodiscard]] btree_storage_kind storage_kind() const noexcept { return storage_kind_; }
    [[nodiscard]] const ElementPolicy& policy() const noexcept { return policy_; }
    [[nodiscard]] std::byte* node_pool() const noexcept { return node_pool_; }
    [[nodiscard]] std::size_t node_stride() const noexcept { return node_stride(policy_.elem_size()); }

    void set_storage_kind(btree_storage_kind kind) noexcept { storage_kind_ = kind; }

    [[nodiscard]] bool full() const noexcept
    {
        if ((static_cast<unsigned>(map_flags_) &
             static_cast<unsigned>(btree_map_policy::fixed_pool)) == 0u) {
            return false;
        }
        return size_ >= node_capacity_;
    }

    void clear() noexcept
    {
        if (root_ != nullptr) {
            clear_subtree(root_);
            root_ = nullptr;
        }
        size_ = 0u;

        if ((static_cast<unsigned>(map_flags_) &
             static_cast<unsigned>(btree_map_policy::fixed_pool)) != 0u &&
            node_pool_ != nullptr) {
            (void)init_free_list();
        }
    }

    [[nodiscard]] status insert(const void* elem) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }

        bool inserted = false;
        const status st = insert_node(&root_, elem, inserted);
        if (ok(st) && inserted) {
            ++size_;
        }
        return st;
    }

    [[nodiscard]] status get(const void* key, void* out_elem) const noexcept
    {
        if (key == nullptr || out_elem == nullptr) {
            return status::null_ptr;
        }

        const btree_node_header* const node = find_node(key);
        if (node == nullptr) {
            return status::not_found;
        }

        std::memcpy(out_elem, node_data(node), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] bool contains(const void* key) const noexcept
    {
        if (key == nullptr) {
            return false;
        }
        return find_node(key) != nullptr;
    }

    [[nodiscard]] status remove(const void* key) noexcept
    {
        if (key == nullptr) {
            return status::null_ptr;
        }

        bool removed = false;
        const status st = remove_node(&root_, key, removed);
        if (!ok(st)) {
            return st;
        }
        if (removed) {
            --size_;
        }
        return status::ok;
    }

    [[nodiscard]] status peek_min(void* out_elem) const noexcept
    {
        if (out_elem == nullptr) {
            return status::null_ptr;
        }
        if (empty()) {
            return status::empty;
        }

        const btree_node_header* node = root_;
        while (node->left != nullptr) {
            node = node->left;
        }

        std::memcpy(out_elem, node_data(node), policy_.elem_size());
        return status::ok;
    }

    [[nodiscard]] status peek_max(void* out_elem) const noexcept
    {
        if (out_elem == nullptr) {
            return status::null_ptr;
        }
        if (empty()) {
            return status::empty;
        }

        const btree_node_header* node = root_;
        while (node->right != nullptr) {
            node = node->right;
        }

        std::memcpy(out_elem, node_data(node), policy_.elem_size());
        return status::ok;
    }

    template<typename Visitor>
    [[nodiscard]] status foreach(btree_traversal order, Visitor&& visit) const
    {
        return traverse_node(root_, order, std::forward<Visitor>(visit));
    }

private:
    [[nodiscard]] int compare_elems(const void* a, const void* b) const noexcept
    {
        return compare_.compare(a, b);
    }

    [[nodiscard]] btree_node_header* pool_node_at(std::size_t index) noexcept
    {
        return reinterpret_cast<btree_node_header*>(node_pool_ + index * node_stride());
    }

    [[nodiscard]] status init_free_list() noexcept
    {
        free_list_ = nullptr;
        for (std::size_t i = 0u; i < node_capacity_; ++i) {
            btree_node_header* slot = pool_node_at(i);
            slot->left              = free_list_;
            slot->right             = nullptr;
            free_list_              = slot;
        }
        return status::ok;
    }

    [[nodiscard]] status acquire_node(btree_node_header** out_node) noexcept
    {
        if ((static_cast<unsigned>(map_flags_) &
             static_cast<unsigned>(btree_map_policy::fixed_pool)) != 0u) {
            if (free_list_ == nullptr) {
                return status::full;
            }

            btree_node_header* slot = free_list_;
            free_list_              = slot->left;
            slot->left              = nullptr;
            slot->right             = nullptr;
            *out_node               = slot;
            return status::ok;
        }

        if (arena_alloc_fn_ != nullptr) {
            void* memory = nullptr;
            const status st =
                arena_alloc_fn_(arena_ctx_, node_stride(), alignof(std::max_align_t), &memory);
            if (!ok(st)) {
                return st;
            }

            btree_node_header* slot = static_cast<btree_node_header*>(memory);
            slot->left              = nullptr;
            slot->right             = nullptr;
            *out_node               = slot;
            return status::ok;
        }

#if MEMKIT_ALLOW_HEAP
        void* memory = std::malloc(node_stride());
        if (memory == nullptr) {
            return status::oom;
        }

        btree_node_header* slot = static_cast<btree_node_header*>(memory);
        slot->left              = nullptr;
        slot->right             = nullptr;
        *out_node               = slot;
        return status::ok;
#else
        (void)out_node;
        return status::unsupported;
#endif
    }

    void release_node(btree_node_header* slot) noexcept
    {
        if (slot == nullptr) {
            return;
        }

        if ((static_cast<unsigned>(map_flags_) &
             static_cast<unsigned>(btree_map_policy::fixed_pool)) != 0u) {
            slot->left  = free_list_;
            slot->right = nullptr;
            free_list_  = slot;
            return;
        }

#if MEMKIT_ALLOW_HEAP
        if (arena_alloc_fn_ == nullptr) {
            std::free(slot);
        }
#endif
    }

    template<typename Arena>
    static status arena_alloc_trampoline(void* ctx, std::size_t size, std::size_t align, void** out)
    {
        return static_cast<Arena*>(ctx)->allocate(size, align, out);
    }

    template<typename Arena>
    void bind_arena(Arena& arena) noexcept
    {
        bind_allocator(&arena, &arena_alloc_trampoline<Arena>);
    }

    [[nodiscard]] status make_node(const void* elem, btree_node_header** out_node) noexcept
    {
        btree_node_header* slot = nullptr;
        const status acquire_st = acquire_node(&slot);
        if (!ok(acquire_st)) {
            return acquire_st;
        }

        if constexpr (std::is_same_v<ElementPolicy, runtime_element_policy>) {
            const status copy_st = policy_.copy_construct(node_data(slot), elem);
            if (!ok(copy_st)) {
                release_node(slot);
                return copy_st;
            }
        } else {
            policy_.copy_construct(node_data(slot), elem);
        }

        *out_node = slot;
        return status::ok;
    }

    void destroy_node_value(btree_node_header* slot) noexcept
    {
        if (policy_.needs_destroy_on_clear()) {
            policy_.destroy(node_data(slot));
        }
    }

    [[nodiscard]] const btree_node_header* find_node(const void* key) const noexcept
    {
        const btree_node_header* current = root_;
        while (current != nullptr) {
            const int cmp = compare_elems(key, node_data(current));
            if (cmp == 0) {
                return current;
            }
            current = cmp < 0 ? current->left : current->right;
        }
        return nullptr;
    }

    [[nodiscard]] status insert_node(btree_node_header** link, const void* elem, bool& inserted) noexcept
    {
        if (*link == nullptr) {
            const status st = make_node(elem, link);
            if (!ok(st)) {
                return st;
            }
            inserted = true;
            return status::ok;
        }

        const int cmp = compare_elems(elem, node_data(*link));
        if (cmp == 0) {
            if constexpr (std::is_same_v<ElementPolicy, runtime_element_policy>) {
                policy_.copy_assign(node_data(*link), elem);
            } else {
                policy_.copy_assign(node_data(*link), elem);
            }
            return status::ok;
        }

        if (cmp < 0) {
            return insert_node(&(*link)->left, elem, inserted);
        }

        return insert_node(&(*link)->right, elem, inserted);
    }

    [[nodiscard]] status remove_node(btree_node_header** link, const void* key, bool& removed) noexcept
    {
        if (*link == nullptr) {
            return status::not_found;
        }

        const int cmp = compare_elems(key, node_data(*link));
        if (cmp < 0) {
            return remove_node(&(*link)->left, key, removed);
        }
        if (cmp > 0) {
            return remove_node(&(*link)->right, key, removed);
        }

        btree_node_header* const target = *link;

        if (target->left != nullptr && target->right != nullptr) {
            btree_node_header** succ_link = &target->right;
            while ((*succ_link)->left != nullptr) {
                succ_link = &(*succ_link)->left;
            }

            btree_node_header* const successor = *succ_link;
            btree_node_header* const saved_right = successor->right;
            *succ_link = saved_right;

            if constexpr (std::is_same_v<ElementPolicy, runtime_element_policy>) {
                const status copy_st =
                    policy_.copy_construct(node_data(target), node_data(successor));
                if (!ok(copy_st)) {
                    successor->right = saved_right;
                    *succ_link       = successor;
                    return copy_st;
                }
            } else {
                policy_.copy_assign(node_data(target), node_data(successor));
            }

            destroy_node_value(successor);
            release_node(successor);
            removed = true;
            return status::ok;
        }

        if (target->left != nullptr) {
            *link = target->left;
        } else {
            *link = target->right;
        }

        destroy_node_value(target);
        release_node(target);
        removed = true;
        return status::ok;
    }

    void clear_subtree(btree_node_header* current) noexcept
    {
        if (current == nullptr) {
            return;
        }

        clear_subtree(current->left);
        clear_subtree(current->right);
        destroy_node_value(current);
        release_node(current);
    }

    template<typename Visitor>
    [[nodiscard]] status traverse_node(
        const btree_node_header* current,
        btree_traversal order,
        Visitor&& visit
    ) const
    {
        if (current == nullptr) {
            return status::ok;
        }

        if (order == btree_traversal::preorder) {
            const status st = visit(node_data(current));
            if (!ok(st)) {
                return st;
            }
        }

        const status left_st = traverse_node(current->left, order, visit);
        if (!ok(left_st)) {
            return left_st;
        }

        if (order == btree_traversal::inorder) {
            const status st = visit(node_data(current));
            if (!ok(st)) {
                return st;
            }
        }

        const status right_st = traverse_node(current->right, order, visit);
        if (!ok(right_st)) {
            return right_st;
        }

        if (order == btree_traversal::postorder) {
            const status st = visit(node_data(current));
            if (!ok(st)) {
                return st;
            }
        }

        return status::ok;
    }

    [[nodiscard]] void* node_data(btree_node_header* node) const noexcept
    {
        const std::size_t header = align_up(sizeof(btree_node_header), alignof(btree_node_header));
        return reinterpret_cast<std::byte*>(node) + header;
    }

    [[nodiscard]] const void* node_data(const btree_node_header* node) const noexcept
    {
        return node_data(const_cast<btree_node_header*>(node));
    }

    ElementPolicy       policy_{};
    ComparePolicy       compare_{};
    btree_node_header*  root_          = nullptr;
    btree_node_header*  free_list_     = nullptr;
    std::byte*          node_pool_     = nullptr;
    std::size_t         size_          = 0u;
    std::size_t         node_capacity_ = 0u;
    btree_map_policy    map_flags_     = btree_map_policy::none;
    btree_storage_kind  storage_kind_  = btree_storage_kind::external;
    void*               arena_ctx_     = nullptr;
    raw_alloc_fn        arena_alloc_fn_ = nullptr;
};

} // namespace memkit::detail
