# Getting started with memkit

This guide walks through the **80% path** most firmware projects need: static storage, init, push/pop, check status. For the full API see [README](../README.md). To pick a container see [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md).

## Pick your path

| You are… | Start here |
|----------|------------|
| C firmware, bare-metal (MCU) | [C on MCU](#c-on-mcu-tier-1) |
| C++ firmware, bare-metal | [C++ on MCU](#c-on-mcu) |
| Embedded Linux (MPU) | [MPU heap / mmap](#mpu-embedded-linux) |
| Choosing a container | [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) |

---

## C++ on MCU

Include one header, use a static array for storage, check `memkit::ok()` on init and mutating calls.

```cpp
#include <memkit/memkit.hpp>

struct sensor_sample {
    std::uint32_t timestamp_ms;
    std::int16_t  value;
};

int main()
{
    memkit::stl::array<sensor_sample, 16> storage{};

    memkit::Queue<sensor_sample> samples;
    if (!memkit::ok(samples.init(storage))) {
        return 1;
    }

    const sensor_sample s{100u, 42};
    if (!memkit::ok(samples.push_back(s))) {
        return 1;
    }

    const auto front = samples.try_peek_front();
    if (front.has_value()) {
        // use front->timestamp_ms, front->value
    }

    return 0;
}
```

**Next steps:** `examples/example_mcu.cpp`, `tests/test_queue_cpp.cpp`, [README C++ API](../README.md#c-api).

---

## C on MCU (tier 1)

Tier 1 C containers: arena, ring, queue, vector, stack, bitset, objpool, handle_pool.

Use [`memkit_helpers.h`](../include/memkit_helpers.h) to skip boilerplate:

```c
#include <memkit.h>
#include <memkit_helpers.h>

typedef struct sensor_sample {
    uint32_t timestamp_ms;
    int16_t  value;
} sensor_sample_t;

int main(void)
{
    MEMKIT_ELEM_STORAGE(sensor_sample_t, 16, queue_buf);

    queue_t queue;
    if (!queue_status_ok(MEMKIT_QUEUE_INIT_STATIC(&queue, sensor_sample_t, queue_buf))) {
        return 1;
    }

    const sensor_sample_t sample = {100u, 42};
    if (!queue_status_ok(queue_push(&queue, &sample))) {
        queue_deinit(&queue);
        return 1;
    }

    sensor_sample_t out = {0};
    if (!queue_status_ok(queue_pop(&queue, &out))) {
        queue_deinit(&queue);
        return 1;
    }

    queue_deinit(&queue);
    return 0;
}
```

### Overwriting ring log (C)

When the buffer is full, drop the oldest entry instead of failing:

```c
MEMKIT_ELEM_STORAGE(sensor_sample_t, 8, log_buf);

ring_t log;
MEMKIT_RING_INIT_STATIC_OVERWRITE(&log, sensor_sample_t, log_buf);
```

### Static arena + create (still no heap)

Allocate several containers from one bump arena:

```c
static uint8_t arena_backing[1024];

arena_t arena;
MEMKIT_ARENA_INIT_STATIC(&arena, arena_backing);

ring_t *ring = NULL;
ring_create(&ring, sizeof(sensor_sample_t), 8u, &arena, RING_FLAG_OVERWRITE_ON_FULL);
/* … use ring … */
ring_destroy(ring);
arena_deinit(&arena);
```

**Next steps:** `examples/example_mcu_c.c`, `tests/test_queue_c.c`, [README C API](../README.md#c-api-1).

---

## MPU (embedded Linux)

MPU builds enable heap, mmap arenas, and tier-2 C containers (hashmap, btree, deque, …).

```bash
make mpu
./build/example_mpu
./build/example_mpu_c
```

Typical MPU pattern — mmap-backed arena and `*_create`:

```c
#include <memkit.h>

arena_t *arena = NULL;
arena_create(&arena, 262144u);

hashmap_t *map = NULL;
hashmap_create(&map, sizeof(uint32_t), sizeof(int32_t), 16u,
               HASHMAP_STRATEGY_CHAINING, arena, HASHMAP_FLAG_GROWABLE);

hashmap_destroy(map);
arena_destroy(arena);
```

C++ MPU: same containers as MCU; optional `memkit::memory::mmap_arena` and growable policies.

**Next steps:** `examples/example_mpu.c`, `examples/example_mpu.cpp`, `tests/test_hashmap_c.c`.

---

## Build and integrate

### Clone and test

```bash
git clone https://github.com/jameslavrenz/memkit.git
cd memkit
make all          # MCU: lib + tests + examples
make mpu          # MPU tier-2 + integration test
```

### CMake (in-tree)

```bash
cmake -B build                           # MCU
cmake -B build -DMEMKIT_EMBEDDED_LINUX=ON   # MPU
cmake --build build
ctest --test-dir build
```

### CMake (FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(
    memkit
    GIT_REPOSITORY https://github.com/jameslavrenz/memkit.git
    GIT_TAG        v0.2.1
)
FetchContent_MakeAvailable(memkit)

add_executable(my_firmware main.c)
target_link_libraries(my_firmware PRIVATE memkit)
target_compile_definitions(my_firmware PRIVATE MEMKIT_MCU=1)
```

Add `-I` paths are handled by the `memkit` target (`target_include_directories` is PUBLIC).

### Firmware checklist

1. Add `-DMEMKIT_MCU=1` (or `MEMKIT_MPU=1` + `EMBEDDED_LINUX=1` on Linux).
2. Link the static `memkit` library (`arena.cpp`, `mmap_backing.cpp`, `c_api/bindings.cpp`).
3. Include `<memkit/memkit.hpp>` (C++) or `<memkit.h>` / individual `*.h` (C).
4. Prefer **static storage** on MCU; use arena when several containers share one buffer.
5. Always check status (`memkit::ok`, `*_status_ok`).

---

## Common mistakes

| Mistake | Fix |
|---------|-----|
| Using `Ring` when you want strict FIFO | Use `Queue` — ring can overwrite; queue returns `FULL` |
| Forgetting `ring_deinit` / `queue_deinit` | Call deinit for `*_init`; `*_destroy` for `*_create` |
| `uint8_t` storage with strict alignment types | Use `MEMKIT_ELEM_STORAGE(type, cap, name)` |
| Tier-2 C API on MCU | Use C++ headers or stick to tier 1 in C |
| Ignoring status codes | Check every init/push/pop; use `memkit_*_status_string` during bring-up |

---

## Where to go next

- [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) — which container for your use case
- [README](../README.md) — complete API reference
- `examples/example_embedded_patterns.cpp` — DMA, MPSC, calibration, bit streams
- `examples/example_comm_pipeline.cpp` — ByteRing + SPSC + TokenBucket
