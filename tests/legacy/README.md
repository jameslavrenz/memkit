# Legacy C container tests

Original pre-refactor C integration tests. They remain useful for caller-owned and arena-backed paths that predate the consolidated `*_box` layer.

**Run via:**

- `make test_c_api_legacy` — after `make mpu` objects exist (or builds them)
- `make mpu` — runs extended C API test, then legacy tests, then examples
- CMake/`ctest` with `-DMEMKIT_EMBEDDED_LINUX=ON` — each `tests/legacy/test_*.c` is a separate `ctest` entry

Primary day-to-day coverage is still:

- `make test_cpp` — C++ container tests (includes heap grow/reallocate paths)
- `make test_c_api_smoke` — quick C API smoke (MCU flags; includes handle pool + arena create)
- `make test_c_api_extended` — tier-1/2 C API on MPU (all `*_create` paths)

Legacy heap `*_create(..., NULL, ...)` tests pre-size capacity to avoid in-place
reallocate during push; C API heap grow is additionally exercised by the C++ suite.

Manual compile (MPU example):

```bash
make mpu   # builds objects
cc -std=c23 -Iinclude -DMEMKIT_MPU=1 -DEMBEDDED_LINUX=1 \
  tests/legacy/test_ring.c build/mpu/arena.o build/mpu/mmap_backing.o build/mpu/c_api/*.o \
  -lstdc++ -o /tmp/test_ring
```
