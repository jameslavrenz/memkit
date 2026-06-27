# Which container?

memkit ships **32 C++ utilities** and **14 C containers**. This guide picks by **what you need to do**, not by class name.

## Quick picker

| I need to… | Container | C API | Notes |
|------------|-----------|-------|-------|
| Buffer sensor samples FIFO, fixed size | `Queue` | `queue_t` | Fails when full; strict FIFO |
| Keep a circular log, drop oldest when full | `Ring` + overwrite policy | `ring_t` | See `RING_FLAG_OVERWRITE_ON_FULL` |
| Push/pop both ends | `Deque` | `deque_t` | MPU / C++ |
| LIFO undo / call stack | `Stack` | `cstack_t` | Same core as vector |
| Growable array, indexed access | `Vector` | `vector_t` | Optional growable on MPU |
| Track a set of flags or IDs | `Bitset` | `bitset_t` | |
| Fixed pool of same-sized objects | `ObjPool` | `objpool_t` | No handles; pointer is the slot |
| Stable ID → object (survives reuse) | `HandlePool` | `handle_pool_t` | Generation-stamped handles |
| Key → value, fast average case | `HashMap` | `hashmap_t` | MPU / C++ |
| Key → value, sorted iteration | `BTree` | `btree_t` | MPU / C++ |
| Priority / schedule by rank | `PQueue` | `pqueue_t` | Binary heap; MPU / C++ |
| Linked list, forward only | `List` | `list_t` | MPU / C++ |
| Linked list, both directions | `DList` | `dlist_t` | MPU / C++ |
| Cache with LRU eviction | `LruCache` | `lrucache_t` | MPU / C++ |
| Raw bytes / UART / DMA chunks | `ByteRing` | — | C++ only |
| ISR → main, one producer & consumer | `SpscQueue` | — | Lock-free; power-of-2 size |
| Multiple ISRs → one consumer | `MpscQueue` | — | Bounded; needs aligned storage |
| Small sorted map (≤ few dozen keys) | `FlatMap` | — | C++ only; cache-friendly |
| Enum → handler table | `EnumMap` | — | C++ only |
| Ping-pong DMA / double buffer | `DoubleBuffer` | — | C++ only |
| Timers / ticks in the future | `TimerWheel` | — | C++ only; intrusive nodes |
| Fixed string, no heap | `SmallString` | — | C++ only |
| Binary payload / protocol frame | `SmallBuffer` | — | C++ only |
| Bump allocate several containers | `arena` / `static_arena` | `arena_t` | Reset whole arena at once |

---

## Ring vs Queue vs Deque vs SPSC vs MPSC

These are the most confused names. All can hold a sequence of elements, but semantics differ:

```
Queue     push back ──► [ oldest … newest ] ──► pop front
          Full → push fails (or growable on MPU)

Ring      push back ──► [ … ] ──► optional overwrite oldest when full
          Good for flight logs, telemetry rings

Deque     push/pop at both ends

SpscQueue one producer, one consumer, lock-free (ISR-safe with correct pairing)

MpscQueue many producers, one consumer, lock-free (bounded)
```

**Rule of thumb**

- **Main loop only, strict FIFO** → `Queue`
- **Fixed log, OK to lose oldest** → `Ring` with overwrite
- **Both ends** → `Deque`
- **Interrupt + thread** → `SpscQueue` or `MpscQueue` (C++ only today)

---

## C vs C++

| Situation | Use |
|-----------|-----|
| `.c` translation units, smallest tier-1 image | C API + `memkit_helpers.h` |
| C++ firmware, all 32 utilities on MCU | `#include <memkit/memkit.hpp>` |
| Mixed codebase | C API from C; C++ templates from C++ |
| Need HashMap/BTree/… in pure C on MCU | Not in tier-1 C — use C++ headers or MPU build |

See [DESIGN_PHILOSOPHY.md](DESIGN_PHILOSOPHY.md) for the full rationale (shared cores, STL policy, linking).

Both APIs share the same `detail/*_core` implementation; behavior matches when configuration is equivalent.

---

## Memory model (when to use what)

Most projects start with **static storage** — a global or stack buffer you pass to `init`.

| Model | When | MCU | MPU |
|-------|------|-----|-----|
| **Static buffer** | Known max size, simplest | ✓ | ✓ |
| **Arena** | Several containers, reset together | ✓ | ✓ |
| **Growable** | Size unknown at compile time | — | ✓ |
| **mmap arena** | Large MPU services | — | ✓ |

See [GETTING_STARTED.md](GETTING_STARTED.md) for code patterns.

---

## Recipes (copy-paste starting points)

### Sensor FIFO (C)

```c
MEMKIT_ELEM_STORAGE(sensor_sample_t, 32, buf);
queue_t q;
MEMKIT_QUEUE_INIT_STATIC(&q, sensor_sample_t, buf);
```

See: `examples/example_mcu_c.c`, `tests/test_queue_c.c`.

### Overwriting telemetry ring (C++)

```cpp
memkit::Ring<sample_t> log;
log.init_from_arena(arena, 64u, memkit::ring_policy::overwrite_on_full);
```

See: `examples/example_mcu.cpp`, `tests/test_ring_cpp.cpp`.

### Connection table with stable IDs (C)

```c
MEMKIT_HANDLE_POOL_STORAGE(conn_t, 8, conns);
handle_pool_t pool;
MEMKIT_HANDLE_POOL_INIT_STATIC(&pool, conn_t, 8, conns);
```

See: `tests/test_handle_pool_c.c`.

### UART byte stream (C++)

```cpp
memkit::ByteRing rx;
rx.init(rx_bytes, 256u);
rx.push_bytes(data, len);
```

See: `examples/example_comm_pipeline.cpp`.

### Config key lookup (MPU C)

```c
hashmap_create(&map, sizeof(uint32_t), sizeof(int32_t), 16u,
               HASHMAP_STRATEGY_CHAINING, arena, HASHMAP_FLAG_GROWABLE);
```

See: `tests/test_hashmap_c.c`.

---

## C++-only utilities (by category)

| Category | Types |
|----------|-------|
| Concurrency | `SpscQueue`, `MpscQueue` |
| I/O & bytes | `ByteRing`, `FixedIoVec`, `BitReader`, `BitWriter` |
| Small helpers | `SmallString`, `SmallBuffer`, `FixedVariant` |
| Maps & tables | `FlatMap`, `EnumMap`, `LookupTable`, `SparseSet` |
| Timing & rate | `TimerWheel`, `TokenBucket`, `RingLog` |
| DMA / signal | `DoubleBuffer`, `MovingAverage`, `WindowStats` |
| Intrusive | `IntrusiveListHead`, hooks |

Full list: [README container cheat sheet](../README.md#container-cheat-sheet).
