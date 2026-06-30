# Distributing memkit for bare-metal C (MCU)

This guide is for shipping **prebuilt objects** so firmware teams can use the **tier-1 C API** without linking **libstdc++** / **libc++**.

## What you ship

| Artifact | Purpose |
|----------|---------|
| `libmemkit_mcu_c.a` | Static library: `arena` + C API bindings only |
| `include/*.h` | C headers (`memkit.h`, `ring.h`, …, `memkit_helpers.h`) |
| `include/memkit_config.h` | Built with `MEMKIT_MCU=1` (or document customer defines) |

**Do not require** `<memkit/memkit.hpp>` for this package — that path needs C++26 STL headers at compile time.

## How the library is built

From the memkit repo:

```bash
make lib-mcu-c              # → build/libmemkit_mcu_c.a
make check-lib-mcu-c        # fail if C++ runtime symbols appear
make test-lib-mcu-c-link    # link example_mcu_c with cc only (no -lstdc++)
```

Compile flags for the archive (see `MCU_C_CXXFLAGS` in the Makefile):

- `-DMEMKIT_MCU=1`
- `-fno-exceptions` and `-fno-rtti` (project-wide on all C++ builds; enforced in Makefile and CMake)
- `-ffreestanding` (this archive only — freestanding tier-1 C delivery)

Rebuild **per target** (e.g. `arm-none-eabi-g++`, `-mcpu=cortex-m4`, `-mthumb`). One host `.a` is not portable across ABIs.

`mmap_backing.cpp` is **omitted** from this archive (MCU has no mmap; the TU is empty under `MEMKIT_ALLOW_MMAP=0`).

## Customer integration (C only)

```bash
# compile firmware (C)
arm-none-eabi-gcc -std=c23 -I/path/to/memkit/include -DMEMKIT_MCU=1 -c app.c

# link — use the C compiler driver, not g++
arm-none-eabi-gcc -o firmware.elf app.o -L/path/to/lib -lmemkit_mcu_c
# + newlib / platform libc (memcpy, memset, …)
# NO -lstdc++ / -lc++
```

Example:

```c
#include <memkit.h>

MEMKIT_ELEM_STORAGE(int, 8, buf);

int main(void)
{
    ring_t ring;
    if (!ring_status_ok(MEMKIT_RING_INIT_STATIC(&ring, int, buf))) {
        return 1;
    }
    int v = 42;
    ring_push_back(&ring, &v);
    ring_deinit(&ring);
    return 0;
}
```

## Link-time expectations

After `make check-lib-mcu-c`, undefined symbols from the archive should be limited to **libc**-style helpers, for example:

- `memcpy`, `memset`, `memcmp`

No `__cxa_*`, `__gxx_*`, or `std::` ABI symbols from libstdc++.

## What this does **not** cover

| Scenario | Requirement |
|----------|-------------|
| Customer uses `#include <memkit/memkit.hpp>` | C++26 + STL **headers** on their compile; separate delivery |
| Tier-2 C API (hashmap, deque, …) | MPU build (`MEMKIT_MPU=1`), not this MCU C package |
| `MpscQueue` / `SpscQueue` / `DoubleBuffer` in customer C++ code | May need `-latomic`; see [CONCURRENCY.md](CONCURRENCY.md) |

The default **`make lib`** and **`make all`** builds use the same **`-fno-exceptions -fno-rtti`** policy as **`lib-mcu-c`**; the freestanding archive adds **`-ffreestanding`** for pure-C customer link tests.

## Verifying a third-party `.a`

```bash
nm -u libmemkit_mcu_c.a | grep -E '__cxa_|__gxx_|_ZSt' && echo BAD || echo OK
```

Any match means the archive was not built with `-fno-exceptions -fno-rtti` (or was linked incorrectly).

## Related docs

- [ADOPTION_GUIDE.md](ADOPTION_GUIDE.md) — STL mapping, MCU/MPU compile flags, ownership, vendoring headers
- [GETTING_STARTED.md](GETTING_STARTED.md) — C API usage
- [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) — tier-1 container picker
