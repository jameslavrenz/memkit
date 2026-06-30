# Concurrency, RTOS integration, and hardware requirements

memkit is **zero-overhead and OS-agnostic**. The library contains **no internal mutexes**, **no RTOS adapters**, and **no virtual dispatch** — architectural choices that maximize execution speed and minimize flash use on bare-metal firmware while remaining portable to RTOS and embedded-Linux deployments.

Containers are split into **two strategic categories**:

| Category | Representative types | Default concurrency |
|----------|---------------------|---------------------|
| **Lock-free utilities** | `SpscQueue`, `MpscQueue`, `DoubleBuffer` (C++ only) | Fixed producer/consumer roles; hardware atomics |
| **Single-threaded containers** | `HashMap`, `Vector`, `BTree`, `FlatMap`, `EnumMap`, `Ring`, `Queue`, C API, `arena`, … | One owning context; integrator adds OS locks if shared |

For design rationale see [DESIGN_PHILOSOPHY.md](DESIGN_PHILOSOPHY.md). For picking a queue type see [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md#ring-vs-queue-vs-deque-vs-spsc-vs-mpsc). Summary also in [README § Zero-overhead architecture](../README.md#zero-overhead--os-agnostic-architecture).

---

## Category 1 — Lock-free trio (C++ only)

Three headers implement **high-performance communication pipelines** — moving data safely across RTOS tasks, or between **Interrupt Service Routines (ISRs)** and the main background loop, without library-internal locking.

| Type | Header | Pattern | Progress guarantee |
|------|--------|---------|-------------------|
| **`SpscQueue<T>`** | [`spsc_queue.hpp`](../include/memkit/containers/spsc_queue.hpp) | Single-producer, single-consumer FIFO (power-of-2 capacity) | **Wait-free** |
| **`MpscQueue<T>`** | [`mpsc_queue.hpp`](../include/memkit/containers/mpsc_queue.hpp) | Multi-producer, **single**-consumer FIFO | **Lock-free** |
| **`DoubleBuffer<T>`** | [`double_buffer.hpp`](../include/memkit/containers/double_buffer.hpp) | Ping-pong buffer: `write_span()` → fill → `publish()` → `read_span()` | **Wait-free** |

**Target use-case:** ISR ↔ task messaging, multi-ISR event fan-in to one consumer task, DMA/ADC/audio block handoff, and any pipeline where **latency and determinism** matter more than generic multi-threaded container semantics.

These types are **not** wrapped in RTOS APIs. You enforce **who calls `push` vs `pop`**, or **`write_span` / `publish` vs `read_span`**. They are not substitutes for `Queue`, `Ring`, or C `queue_t`.

### Progress guarantees (terminology)

| Guarantee | Meaning in practice |
|-----------|---------------------|
| **Wait-free** | Each participating thread/ISR completes its operation in a **bounded** number of steps when roles are respected — no retry loops waiting on other contexts (`SpscQueue`, `DoubleBuffer`). |
| **Lock-free** | System-wide progress is guaranteed; an individual producer may **spin briefly** contending for a slot (`MpscQueue` — Vyukov-style sequence cells). |

### Role contract

| Type | Producers | Consumers | Carries |
|------|-----------|-----------|---------|
| `SpscQueue` | Exactly **one** (task or ISR) | Exactly **one** | Discrete messages |
| `MpscQueue` | **Many** (tasks and/or ISRs) | Exactly **one** | Discrete messages |
| `DoubleBuffer` | Exactly **one** | Exactly **one** | One **full block** per `publish()` |

### How they differ from `Queue` / `Ring`

| | **`Queue` / `queue_t`** | **`Ring` / `ring_t`** | **`SpscQueue` / `MpscQueue`** | **`DoubleBuffer`** |
|--|-------------------------|----------------------|----------------------------------|--------------------|
| **Cross-context safe?** | No — single-threaded | No — single-threaded | Yes, with correct pairing | Yes, with correct pairing |
| **C API?** | Yes | Yes | No | No |
| **Typical use** | Task-local FIFO | Telemetry / flight log | ISR → task messages | DMA / ADC frame snapshot |

**Common mistake:** calling C `queue_push` or C++ `Queue::push_back` from an ISR because the name says “queue.” Use the **lock-free trio**, an **RTOS queue**, or **single-task ownership**.

### Implementation notes

- **`SpscQueue`:** atomic head/tail; optional drop-on-full / overwrite-on-full policies (see header).
- **`MpscQueue`:** per-cell sequence atomics; storage must meet `MpscQueue<T>::storage_align()`. Arena allocation uses absolute-address alignment ([bounds and sizing](DESIGN_PHILOSOPHY.md#bounds-and-sizing-what-memkit-checks-vs-what-you-own)).
- **`DoubleBuffer`:** single atomic index selects readable slot; producer must not touch the read slot during fill.

Examples: `examples/example_embedded_patterns.cpp` (DoubleBuffer, MpscQueue), `examples/example_comm_pipeline.cpp` (SpscQueue).

---

## Category 2 — Single-threaded containers (bare-metal optimized)

All other memkit types — **`HashMap`**, **`Vector`**, **`BTree`**, **`FlatMap`**, **`EnumMap`**, **`Ring`**, **`Queue`**, **`ObjPool`**, **`ByteRing`**, the full **C API** (`ring_t`, `queue_t`, `vector_t`, …), and **`arena` / `arena_t`** — are **raw, ultra-fast, and strictly single-threaded**.

There is **no hidden locking**. Two RTOS tasks (or an ISR and a task) mutating the same instance without a contract is **undefined behavior**.

**Architectural advantage:** zero synchronization overhead on every push, pop, lookup, or arena bump — ideal when a container is owned by one task or one main loop.

### RTOS integration — application-layer locking

When a single-threaded container **must** be shared across tasks, wrap access with **your** OS-native primitive: mutex, semaphore, or critical section. memkit deliberately stays OS-agnostic so you pick the mechanism that fits latency, priority inversion policy, and ISR rules.

**Generic C++ wrapper pattern:**

```cpp
#include <memkit/memkit.hpp>
#include "FreeRTOS.h"
#include "semphr.h"

template<typename Fn>
memkit::status with_lock(SemaphoreHandle_t mu, Fn&& fn)
{
    if (xSemaphoreTake(mu, portMAX_DELAY) != pdTRUE) {
        return memkit::status::invalid;
    }
    const memkit::status st = fn();
    xSemaphoreGive(mu);
    return st;
}

static memkit::HashMap<std::uint32_t, config_t> g_cfg;
static SemaphoreHandle_t g_cfg_mu;

memkit::status cfg_put(std::uint32_t key, const config_t& value)
{
    return with_lock(g_cfg_mu, [&] { return g_cfg.put(key, value); });
}
```

**C API — same idea:**

```c
#include <queue.h>
#include "semphr.h"

static queue_t g_queue;
static SemaphoreHandle_t g_queue_mu;

queue_status_t queue_push_threadsafe(const void *elem)
{
    if (xSemaphoreTake(g_queue_mu, pdMS_TO_TICKS(10)) != pdTRUE) {
        return QUEUE_ERR_FULL;
    }
    const queue_status_t st = queue_push(&g_queue, elem);
    xSemaphoreGive(g_queue_mu);
    return st;
}
```

**Pure C firmware** without C++: tier-1 C has **no** lock-free queue. Options — RTOS queue for ISR handoff, a small C++ translation unit using the trio, or **single-task ownership**.

---

## Hardware & ISA compatibility (lock-free trio only)

Lock-free utilities compile against `std::atomic`, which must lower to **native exclusive load/store** (or equivalent) for true lock-free behavior. **Single-threaded containers are unaffected** — they run on every MCU target memkit supports, including Cortex-M0.

| Platform | Lock-free trio | Details |
|----------|:--------------:|---------|
| **ARM Cortex-M0 / M0+** (ARMv6-M) | **Not supported** | Ultra-low-power cores **lack** exclusive monitor instructions (`LDREX`/`STREX` and ARMv8-M equivalents). Software atomics cannot provide the same hardware-backed guarantees. **Single-threaded containers work perfectly.** |
| **ARM Cortex-M3 / M4 / M4F / M7** (ARMv7-M) | **Fully supported** | Native **`LDREX` / `STREX`** — 100% lock-free execution on supported hot paths. |
| **ARM Cortex-M33** (ARMv8-M) | **Highly optimized** | **`LDAEX` / `STLEX`** with integrated barriers — hardware enforces memory ordering for maximum efficiency. |
| **Desktop / server** (x86, x64, ARM A-series) | **Fully supported** | Native atomic instructions; used for host tests, CI, and MPU builds. |

**Toolchain:** on some embedded GCC targets, link **` -latomic`** when using the lock-free trio. See [DISTRIBUTING_MCU_C.md](DISTRIBUTING_MCU_C.md).

**Do not** use `SpscQueue`, `MpscQueue`, or `DoubleBuffer` on Cortex-M0/M0+ expecting lock-free ISR handoff — use single-threaded containers, an RTOS message queue, or a single-context design.

---

## FreeRTOS patterns (lock-free trio)

memkit does **not** depend on FreeRTOS. Snippets below illustrate typical integration.

### ISR → task with `SpscQueue` (one interrupt source)

```cpp
#include <memkit/memkit.hpp>
#include "FreeRTOS.h"
#include "task.h"

struct event_t { std::uint8_t source; std::uint16_t value; };

static memkit::SpscQueue<event_t> g_events;
static TaskHandle_t g_consumer_task = nullptr;

extern "C" void UART_IRQHandler(void)
{
    const event_t ev{.source = 1u, .value = read_uart_dr()};
    if (memkit::ok(g_events.push(ev))) {
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(g_consumer_task, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

void consumer_task(void*)
{
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        event_t ev{};
        while (memkit::ok(g_events.pop(ev))) {
            handle_event(ev);
        }
    }
}
```

### Multiple ISRs → one task with `MpscQueue`

Each ISR may `push`; **only** the consumer task calls `pop`.

```cpp
static memkit::MpscQueue<event_t> g_events;

extern "C" void Timer_ISR(void)
{
    (void)g_events.push(event_t{.source = 2u, .value = 0u});
}

void consumer_task(void*)
{
    for (;;) {
        event_t ev{};
        if (memkit::ok(g_events.pop(ev))) {
            handle_event(ev);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
```

### DMA / ADC with `DoubleBuffer`

```cpp
static memkit::DoubleBuffer<adc_frame_t> g_adc;

void dma_half_complete_isr(void)
{
    memkit::stl::span<adc_frame_t> w = g_adc.write_span();
    fill_from_dma(w[0]);
    g_adc.publish();
}

void signal_task(void*)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        memkit::stl::span<const adc_frame_t> r = g_adc.read_span();
        process_frame(r[0]);
    }
}
```

---

## MPU / embedded Linux

The same two-category model applies on MPU builds. memkit does not add `pthread` locks. Share single-threaded containers with **`pthread_mutex`** (or your framework lock). The lock-free trio still requires fixed producer/consumer roles — pthreads do not make `SpscQueue` a general concurrent container.

---

## Quick decision chart

```
Need ISR or multiple contexts?
│
├─ No  → Single-threaded container — one task, or your OS mutex
│
└─ Yes → On supported hardware (not Cortex-M0)?
         ├─ Discrete messages, one producer  → SpscQueue (wait-free)
         ├─ Discrete messages, many producers → MpscQueue (lock-free, one consumer)
         └─ One block snapshot (DMA/ADC)      → DoubleBuffer (wait-free)

Cortex-M0 / M0+ or pure C ISR handoff?
         → RTOS queue, or single-threaded design
```

---

## Related docs

- [README § Zero-overhead architecture](../README.md#zero-overhead--os-agnostic-architecture)
- [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) — which container for which job
- [DESIGN_PHILOSOPHY.md](DESIGN_PHILOSOPHY.md) — MCU vs MPU, memory models
- [ADOPTION_GUIDE.md](ADOPTION_GUIDE.md) — build flags, `-latomic`, piecemeal integration
- [CXX_API_REFERENCE.md](CXX_API_REFERENCE.md) — API tables for the lock-free trio
