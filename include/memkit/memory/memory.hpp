#pragma once

#include "arena.hpp"
#include "fixed_buffer.hpp"
#include "heap.hpp"
#include "mmap.hpp"

namespace memkit::memory {

#if MEMKIT_ALLOW_HEAP
using heap_arena = arena<heap_storage>;
#endif

#if MEMKIT_ALLOW_MMAP
using mmap_arena = arena<mmap_storage>;
#endif

using static_arena = arena<fixed_buffer>;

} // namespace memkit::memory
