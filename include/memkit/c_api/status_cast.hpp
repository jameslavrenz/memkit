#pragma once

#include "../status.hpp"

#include "../../bitset.h"
#include "../../dlist.h"
#include "../../hashmap.h"
#include "../../list.h"
#include "../../lrucache.h"
#include "../../handle_pool.h"
#include "../../objpool.h"
#include "../../pqueue.h"
#include "../../btree.h"

namespace memkit::c_api {

[[nodiscard]] inline bitset_status_t to_bitset_status(status st) noexcept
{
    switch (st) {
    case status::ok:         return BITSET_OK;
    case status::null_ptr:     return BITSET_ERR_NULL;
    case status::invalid:    return BITSET_ERR_INVALID;
    case status::not_found:  return BITSET_ERR_NOT_FOUND;
    case status::empty:      return BITSET_ERR_EMPTY;
    case status::full:       return BITSET_ERR_FULL;
    case status::oom:        return BITSET_ERR_OOM;
    case status::unsupported: return BITSET_ERR_UNSUPPORTED;
    }
    return BITSET_ERR_INVALID;
}

[[nodiscard]] inline objpool_status_t to_objpool_status(status st) noexcept
{
    switch (st) {
    case status::ok:         return OBJPOOL_OK;
    case status::null_ptr:     return OBJPOOL_ERR_NULL;
    case status::invalid:    return OBJPOOL_ERR_INVALID;
    case status::not_found:  return OBJPOOL_ERR_NOT_FOUND;
    case status::empty:      return OBJPOOL_ERR_EMPTY;
    case status::full:       return OBJPOOL_ERR_FULL;
    case status::oom:        return OBJPOOL_ERR_OOM;
    case status::unsupported: return OBJPOOL_ERR_UNSUPPORTED;
    }
    return OBJPOOL_ERR_INVALID;
}

[[nodiscard]] inline pqueue_status_t to_pqueue_status(status st) noexcept
{
    switch (st) {
    case status::ok:         return PQUEUE_OK;
    case status::null_ptr:     return PQUEUE_ERR_NULL;
    case status::invalid:    return PQUEUE_ERR_INVALID;
    case status::empty:      return PQUEUE_ERR_EMPTY;
    case status::full:       return PQUEUE_ERR_FULL;
    case status::oom:        return PQUEUE_ERR_OOM;
    case status::unsupported: return PQUEUE_ERR_UNSUPPORTED;
    default:                 return PQUEUE_ERR_INVALID;
    }
}

[[nodiscard]] inline list_status_t to_list_status(status st) noexcept
{
    switch (st) {
    case status::ok:         return LIST_OK;
    case status::null_ptr:     return LIST_ERR_NULL;
    case status::invalid:    return LIST_ERR_INVALID;
    case status::not_found:  return LIST_ERR_NOT_FOUND;
    case status::empty:      return LIST_ERR_EMPTY;
    case status::full:       return LIST_ERR_FULL;
    case status::oom:        return LIST_ERR_OOM;
    case status::unsupported: return LIST_ERR_UNSUPPORTED;
    }
    return LIST_ERR_INVALID;
}

[[nodiscard]] inline dlist_status_t to_dlist_status(status st) noexcept
{
    switch (st) {
    case status::ok:         return DLIST_OK;
    case status::null_ptr:     return DLIST_ERR_NULL;
    case status::invalid:    return DLIST_ERR_INVALID;
    case status::not_found:  return DLIST_ERR_NOT_FOUND;
    case status::empty:      return DLIST_ERR_EMPTY;
    case status::full:       return DLIST_ERR_FULL;
    case status::oom:        return DLIST_ERR_OOM;
    case status::unsupported: return DLIST_ERR_UNSUPPORTED;
    }
    return DLIST_ERR_INVALID;
}

[[nodiscard]] inline hashmap_status_t to_hashmap_status(status st) noexcept
{
    switch (st) {
    case status::ok:          return HASHMAP_OK;
    case status::null_ptr:      return HASHMAP_ERR_NULL;
    case status::invalid:     return HASHMAP_ERR_INVALID;
    case status::not_found:   return HASHMAP_ERR_NOT_FOUND;
    case status::empty:       return HASHMAP_ERR_INVALID;
    case status::full:        return HASHMAP_ERR_FULL;
    case status::oom:         return HASHMAP_ERR_OOM;
    case status::unsupported: return HASHMAP_ERR_UNSUPPORTED;
    }
    return HASHMAP_ERR_INVALID;
}

[[nodiscard]] inline btree_status_t to_btree_status(status st) noexcept
{
    switch (st) {
    case status::ok:          return BTREE_OK;
    case status::null_ptr:    return BTREE_ERR_NULL;
    case status::invalid:     return BTREE_ERR_INVALID;
    case status::not_found:   return BTREE_ERR_NOT_FOUND;
    case status::empty:       return BTREE_ERR_EMPTY;
    case status::full:        return BTREE_ERR_FULL;
    case status::oom:         return BTREE_ERR_OOM;
    case status::unsupported: return BTREE_ERR_UNSUPPORTED;
    }
    return BTREE_ERR_INVALID;
}

    [[nodiscard]] inline lrucache_status_t to_lrucache_status(status st) noexcept
    {
        switch (st) {
        case status::ok:          return LRUCACHE_OK;
        case status::null_ptr:    return LRUCACHE_ERR_NULL;
        case status::invalid:     return LRUCACHE_ERR_INVALID;
        case status::not_found:   return LRUCACHE_ERR_NOT_FOUND;
        case status::full:        return LRUCACHE_ERR_FULL;
        case status::oom:         return LRUCACHE_ERR_OOM;
        case status::unsupported: return LRUCACHE_ERR_UNSUPPORTED;
        default:                  return LRUCACHE_ERR_INVALID;
        }
    }

    [[nodiscard]] inline handle_pool_status_t to_handle_pool_status(status st) noexcept
    {
        switch (st) {
        case status::ok:          return HANDLE_POOL_OK;
        case status::null_ptr:    return HANDLE_POOL_ERR_NULL;
        case status::invalid:     return HANDLE_POOL_ERR_INVALID;
        case status::empty:       return HANDLE_POOL_ERR_EMPTY;
        case status::full:        return HANDLE_POOL_ERR_FULL;
        case status::oom:         return HANDLE_POOL_ERR_OOM;
        case status::unsupported: return HANDLE_POOL_ERR_UNSUPPORTED;
        default:                  return HANDLE_POOL_ERR_INVALID;
        }
    }

} // namespace memkit::c_api
