#pragma once

#include "../detail/intrusive_hook.hpp"
#include "../status.hpp"

namespace memkit {

/** Intrusive singly-linked list head (embed hooks in your nodes). */
using IntrusiveForwardListHead = detail::intrusive_forward_list_head;

/** Intrusive circular doubly-linked list head (Linux list_head style). */
using IntrusiveListHead = detail::intrusive_list_head;

using IntrusiveForwardHook = detail::intrusive_forward_hook;
using IntrusiveListHook    = detail::intrusive_list_hook;
using IntrusiveDlistHook   = detail::intrusive_dlist_hook;

template<typename Parent, typename Hook, Hook Parent::* Member>
[[nodiscard]] inline Parent* container_from_hook(Hook* hook) noexcept
{
    return detail::container_from_hook<Parent, Hook, Member>(hook);
}

template<typename Parent, typename Hook, Hook Parent::* Member>
[[nodiscard]] inline const Parent* container_from_hook(const Hook* hook) noexcept
{
    return detail::container_from_hook<Parent, Hook, Member>(hook);
}

} // namespace memkit
