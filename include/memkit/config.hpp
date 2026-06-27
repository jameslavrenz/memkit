#pragma once

#include "../memkit_config.h"

#include <cstddef>

namespace memkit {

/** Minimum alignment for arena blocks that may hold atomics or pointers (Unix 64-bit). */
inline constexpr std::size_t storage_alignment = 8u;

inline constexpr bool target_mcu = MEMKIT_TARGET_MCU != 0;
inline constexpr bool target_mpu = MEMKIT_TARGET_MPU != 0;
inline constexpr bool c_api_full      = MEMKIT_C_API_FULL != 0;
inline constexpr bool c_api_extended  = MEMKIT_C_API_EXTENDED != 0;
inline constexpr bool allow_heap = MEMKIT_ALLOW_HEAP != 0;
inline constexpr bool allow_mmap = MEMKIT_ALLOW_MMAP != 0;
inline constexpr bool allow_zero_cost_stl = true;
inline constexpr bool allow_heap_stl = MEMKIT_ALLOW_HEAP_STL != 0;
inline constexpr bool use_stl          = allow_heap_stl;

enum class memory_model : unsigned {
    fixed_buffer = MEMKIT_MEMORY_FIXED_BUFFER,
    fixed_pool   = MEMKIT_MEMORY_FIXED_POOL,
    arena        = MEMKIT_MEMORY_ARENA,
    heap         = MEMKIT_MEMORY_HEAP,
    mmap         = MEMKIT_MEMORY_MMAP,
};

inline constexpr memory_model default_arena_backing =
    static_cast<memory_model>(MEMKIT_DEFAULT_ARENA_BACKING);

} // namespace memkit
