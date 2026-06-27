#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include <memkit/memkit.hpp>

extern "C" {
#include "hand_rolled/ring_buffer.h"
#include "hand_rolled/fifo_queue.h"
}

namespace {

std::size_t bench_iterations()
{
    if (const char* env = std::getenv("MEMKIT_BENCH_ITERS")) {
        const long parsed = std::strtol(env, nullptr, 10);
        if (parsed > 0) {
            return static_cast<std::size_t>(parsed);
        }
    }
    return 200000u;
}

template<typename Fn>
long long measure_ns(Fn&& fn)
{
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

void bench_ring(std::size_t iters)
{
    memkit::stl::array<int, 256> mk_storage{};
    memkit::Ring<int> mk_ring;
    (void)mk_ring.init(mk_storage);

    int32_t c_storage[256];
    c_ring_t c_ring;
    c_ring_init(&c_ring, c_storage, 256u);

    volatile int sink = 0;

    const long long mk_ns = measure_ns([&]() {
        for (std::size_t i = 0u; i < iters; ++i) {
            (void)mk_ring.push_back(static_cast<int>(i & 0x7FFFFFFF));
            int out = 0;
            (void)mk_ring.pop_front(&out);
            sink += out;
        }
    });

    const long long c_ns = measure_ns([&]() {
        for (std::size_t i = 0u; i < iters; ++i) {
            (void)c_ring_push_back(&c_ring, static_cast<int32_t>(i & 0x7FFFFFFF));
            int32_t out = 0;
            (void)c_ring_pop_front(&c_ring, &out);
            sink += out;
        }
    });

    std::printf(
        "ring push/pop x%zu: memkit %.2f ms (%.0f ns/op) | hand-rolled C %.2f ms (%.0f ns/op) | ratio %.2fx\n",
        iters,
        static_cast<double>(mk_ns) / 1.0e6,
        static_cast<double>(mk_ns) / static_cast<double>(iters * 2u),
        static_cast<double>(c_ns) / 1.0e6,
        static_cast<double>(c_ns) / static_cast<double>(iters * 2u),
        static_cast<double>(mk_ns) / static_cast<double>(c_ns)
    );
    (void)sink;
}

void bench_queue(std::size_t iters)
{
    memkit::stl::array<int, 256> mk_storage{};
    memkit::Queue<int> mk_queue;
    (void)mk_queue.init(mk_storage);

    int32_t c_storage[256];
    c_fifo_t c_queue;
    c_fifo_init(&c_queue, c_storage, 256u);

    volatile int sink = 0;

    const long long mk_ns = measure_ns([&]() {
        for (std::size_t i = 0u; i < iters; ++i) {
            (void)mk_queue.push_back(static_cast<int>(i & 0x7FFFFFFF));
            int out = 0;
            (void)mk_queue.pop_front(&out);
            sink += out;
        }
    });

    const long long c_ns = measure_ns([&]() {
        for (std::size_t i = 0u; i < iters; ++i) {
            (void)c_fifo_push(&c_queue, static_cast<int32_t>(i & 0x7FFFFFFF));
            int32_t out = 0;
            (void)c_fifo_pop(&c_queue, &out);
            sink += out;
        }
    });

    std::printf(
        "queue push/pop x%zu: memkit %.2f ms (%.0f ns/op) | hand-rolled C %.2f ms (%.0f ns/op) | ratio %.2fx\n",
        iters,
        static_cast<double>(mk_ns) / 1.0e6,
        static_cast<double>(mk_ns) / static_cast<double>(iters * 2u),
        static_cast<double>(c_ns) / 1.0e6,
        static_cast<double>(c_ns) / static_cast<double>(iters * 2u),
        static_cast<double>(mk_ns) / static_cast<double>(c_ns)
    );
    (void)sink;
}

} // namespace

int main()
{
    const std::size_t iters = bench_iterations();
    std::printf("memkit benchmark (MCU host run, iters=%zu)\n", iters);
    bench_ring(iters);
    bench_queue(iters);
    return 0;
}
