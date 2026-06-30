# Concurrency and RTOS usage

memkit is **MCU-first** and **RTOS-agnostic**. It does not ship mutexes, semaphores, critical-section helpers, or bindings for FreeRTOS, Zephyr, ThreadX, or any other OS. For almost all containers, **you choose who may call what** — or you add your own RTOS synchronization.

For design rationale see [DESIGN_PHILOSOPHY.md](DESIGN_PHILOSOPHY.md). For picking a queue type see [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md#ring-vs-queue-vs-deque-vs-spsc-vs-mpsc).

---

## Default contract: single context

Unless stated otherwise below, treat every memkit type as **single-context**:

- One FreeRTOS task owns it, **or**
- One “main loop” with no concurrent access, **or**
- You wrap all mutating calls in **your** mutex / critical section

There is **no hidden locking** inside `Ring`, `Queue`, `Vector`, `HashMap`, `ObjPool`, `arena`, or the C API (`ring_t`, `queue_t`, …). Two tasks (or an ISR and a task) calling `push` / `pop` / `alloc` on the same instance without a contract is **undefined behavior**, same as two C functions racing on a shared buffer.

This is intentional: no RTOS coupling, no lock overhead on hot paths, predictable code size on Cortex-M.

---

## The lock-free trio (C++ only)

Three C++ utilities use `std::atomic` for **cross-context handoff** with fixed roles. They are **not** general “thread-safe containers” — they solve specific producer/consumer patterns.

| Type | Who may produce | Who may consume | What it carries | Not lock-free for |
|------|-----------------|-----------------|-----------------|-------------------|
| **`SpscQueue<T>`** | Exactly **one** producer (task or ISR) | Exactly **one** consumer (task or ISR) | Discrete messages (power-of-2 capacity) | Multiple producers or consumers |
| **`MpscQueue<T>`** | **Many** producers (tasks and/or ISRs) | Exactly **one** consumer | Discrete messages (power-of-2; aligned storage) | Multiple consumers; unbounded blocking |
| **`DoubleBuffer<T>`** | Exactly **one** producer | Exactly **one** consumer | One **full slot** (array/block), not a stream of items | Queue semantics; multiple readers |

### How they differ from `Queue` / `Ring`

| | **`Queue` / `queue_t`** | **`Ring` / `ring_t`** | **`SpscQueue` / `MpscQueue`** | **`DoubleBuffer`** |
|--|-------------------------|----------------------|----------------------------------|--------------------|
| **Thread / ISR safe?** | No — single context | No — single context | Yes, **only** with correct pairing | Yes, **only** with correct pairing |
| **C API?** | Yes (tier 1) | Yes (tier 1) | No — C++ only | No — C++ only |
| **Full when…** | Push fails (`FULL`) unless MPU growable | Push fails or overwrites oldest (policy) | Push fails (`FULL`) or spins then fails (MPSC) | Producer must finish slot before `publish()` |
| **Typical use** | Task-local FIFO | Telemetry / flight log | ISR → task messages | DMA / ADC ping-pong block |

**Common mistake:** using C `queue_t` or C++ `Queue` from an ISR because the name says “queue.” Those types are **not** concurrent. For ISR → task messaging, use **`SpscQueue`** or **`MpscQueue`** (C++), or your RTOS queue, or a C++ wrapper around memkit’s lock-free types.

### `SpscQueue` details

- Lock-free single-producer / single-consumer FIFO.
- Capacity must be a **power of two**.
- Producer calls `push`; consumer calls `pop`. Do not call both sides from the same context unless you serialize externally.
- Optional policies: drop-on-full, overwrite-on-full (see header).
- May require **`-latomic`** on some embedded GCC toolchains.

### `MpscQueue` details

- Vyukov-style bounded MPSC queue: many `push`, one `pop`.
- Storage must satisfy `MpscQueue<T>::storage_align()` (atomics alignment). Arena allocation handles absolute-address alignment; see [bounds and sizing](DESIGN_PHILOSOPHY.md#bounds-and-sizing-what-memkit-checks-vs-what-you-own).
- `push` may spin up to a limit then return `full`; `pop` is **not** safe from multiple consumers.
- **Not** multi-consumer — do not call `pop` from two tasks.

### `DoubleBuffer` details

- Two slots: producer fills the **write** slot (`write_span()`), then `publish()`; consumer reads the stable **read** slot (`read_span()`).
- Carries a **snapshot** of a block (e.g. one ADC frame), not individual queued events.
- Producer must not touch the read slot; consumer must not touch the write slot until after `publish()`.

See `examples/example_embedded_patterns.cpp` (DoubleBuffer, MpscQueue) and `examples/example_comm_pipeline.cpp` (SpscQueue).

---

## What is not concurrent

| Surface | Safe across tasks/ISRs without your lock? |
|---------|-------------------------------------------|
| C API tier 1 & 2 (`ring_t`, `queue_t`, …) | **No** |
| C++ `Ring`, `Queue`, `Deque`, `Vector`, maps, pools, … | **No** |
| `arena` / `arena_t` bump allocation | **No** |
| `ByteRing`, `TimerWheel`, `HandlePool`, … | **No** |
| Growable / `*_create` paths (MPU) | **No** |

**Arena:** one task (or critical section) should `allocate` / `reset`. Sharing an arena across tasks without external serialization is unsafe.

**Pure C firmware:** tier-1 C has **no** lock-free queue. Options:

1. Use C++ for the ISR handoff module (`SpscQueue` / `MpscQueue`) and keep the rest in C.
2. Use your RTOS’s native queue for cross-context messages.
3. Keep shared memkit C containers on **one task only**.

---

## When to use your RTOS lock

Use **your** mutex, semaphore, or critical section when:

- Two or more tasks share the same `Queue`, `HashMap`, `ObjPool`, etc.
- One task initializes or resets an **arena** while others might allocate.
- You need **multiple consumers** — memkit’s MPSC queue does not support that; merge in one task or use RTOS primitives.
- You wrap memkit in a driver API used from unpredictable call sites.

memkit will not pick the lock type for you (ISR-safe mutex vs task mutex vs `taskENTER_CRITICAL`). That depends on your RTOS and latency budget.

---

## FreeRTOS patterns (illustrative)

memkit does **not** depend on FreeRTOS. The snippets below show typical **integration** — copy and adapt naming/handles to your project.

### Pattern 1: ISR → task with `SpscQueue` (recommended for one interrupt source)

Use the queue for data; use a task notification (or binary semaphore) only to **wake** the consumer — avoid copying large payloads through `xQueueSend`.

```cpp
#include <memkit/memkit.hpp>
#include "FreeRTOS.h"
#include "task.h"

struct event_t { std::uint8_t source; std::uint16_t value; };

static memkit::SpscQueue<event_t> g_events;
static TaskHandle_t g_consumer_task = nullptr;

// Call once at startup (consumer task context)
void events_init(std::byte* storage, std::size_t cap_pow2)
{
    memkit::ok(g_events.init(storage, cap_pow2));
}

// UART ISR — sole producer
extern "C" void UART_IRQHandler(void)
{
    const event_t ev{.source = 1u, .value = read_uart_dr()};
    if (memkit::ok(g_events.push(ev))) {
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(g_consumer_task, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

// Consumer task — sole consumer
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

### Pattern 2: Multiple ISRs → one task with `MpscQueue`

Each ISR may `push`; **only** the consumer task calls `pop`.

```cpp
static memkit::MpscQueue<event_t> g_events;

extern "C" void Timer_ISR(void)
{
    const event_t ev{.source = 2u, .value = 0u};
    (void)g_events.push(ev);  // check status if drops matter
}

extern "C" void GPIO_ISR(void)
{
    const event_t ev{.source = 3u, .value = pin_state()};
    (void)g_events.push(ev);
}

void consumer_task(void*)
{
    for (;;) {
        event_t ev{};
        if (memkit::ok(g_events.pop(ev))) {
            handle_event(ev);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));  // or wait on notification when idle
        }
    }
}
```

### Pattern 3: Two tasks sharing a memkit `Queue` — use a mutex

`Queue` / `queue_t` is **not** ISR-safe and **not** lock-free. Serialize with FreeRTOS:

```cpp
#include <memkit/memkit.hpp>
#include "semphr.h"

static memkit::Queue<sample_t> g_samples;
static SemaphoreHandle_t g_samples_mu = nullptr;

void samples_init(/* storage */)
{
    g_samples_mu = xSemaphoreCreateMutex();
    memkit::ok(g_samples.init(storage));
}

memkit::status sample_push(const sample_t* s)
{
    if (xSemaphoreTake(g_samples_mu, pdMS_TO_TICKS(10)) != pdTRUE) {
        return memkit::status::full;  // or your own timeout policy
    }
    const memkit::status st = g_samples.push_back(*s);
    xSemaphoreGive(g_samples_mu);
    return st;
}
```

Same idea for C API:

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

### Pattern 4: DMA / ADC with `DoubleBuffer`

Producer (ISR or DMA completion) fills write slot, then publishes; consumer task reads read slot.

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

The same contract applies on MPU builds: memkit does not add pthread locks. If several pthreads share a container, use `pthread_mutex` (or your framework’s lock) around mutating calls. The lock-free trio still requires the same producer/consumer pairing; pthreads do not turn `SpscQueue` into a general concurrent queue.

---

## Toolchain note

`SpscQueue`, `MpscQueue`, and `DoubleBuffer` use `std::atomic`. On some embedded GCC targets you may need **`-latomic`**. See [DISTRIBUTING_MCU_C.md](DISTRIBUTING_MCU_C.md) if C++ firmware links these types.

---

## Quick decision chart

```
Need ISR or multiple contexts?
│
├─ No  → Queue / Ring / Vector / C API — single task or your mutex
│
└─ Yes → Discrete messages?
         ├─ One producer  → SpscQueue
         ├─ Many producers, one consumer → MpscQueue
         └─ One block snapshot (DMA/ADC) → DoubleBuffer

Pure C only, ISR handoff?
         → RTOS queue, or C++ module with SpscQueue/MpscQueue
```

---

## Related docs

- [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) — which container for which job
- [DESIGN_PHILOSOPHY.md](DESIGN_PHILOSOPHY.md) — MCU vs MPU, memory models
- [ADOPTION_GUIDE.md](ADOPTION_GUIDE.md) — build flags, `-latomic`, piecemeal integration
- [CXX_API_REFERENCE.md](CXX_API_REFERENCE.md) — `SpscQueue`, `MpscQueue`, `DoubleBuffer` API tables
