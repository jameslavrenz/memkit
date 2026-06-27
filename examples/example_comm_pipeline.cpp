/*
 * UART-style pipeline: ByteRing RX buffer, SPSC event queue, TokenBucket pacing.
 * Simulates ISR byte capture and main-loop frame dispatch on MCU.
 */

#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/memkit.hpp>

struct frame_event {
    std::uint8_t len;
    std::uint8_t bytes[4];
};

static void simulate_uart_rx(memkit::ByteRing& rx, const std::uint8_t* data, std::size_t count)
{
    std::size_t written = 0u;
    assert(memkit::ok(rx.push_bytes(data, count, &written)));
}

static void parse_frames(
    memkit::ByteRing& rx,
    memkit::SpscQueue<frame_event>& events
)
{
    while (!rx.empty()) {
        std::uint8_t byte = 0u;
        if (rx.pop_byte(&byte) != memkit::status::ok) {
            break;
        }

        static frame_event partial{};
        partial.bytes[partial.len++] = byte;

        if (partial.len >= 3u) {
            frame_event complete = partial;
            partial.len          = 0u;
            (void)events.push(complete);
        }
    }
}

int main()
{
    memkit::stl::array<std::byte, 64> rx_backing{};
    memkit::ByteRing rx;
    assert(memkit::ok(rx.init(rx_backing, 32u)));

    memkit::stl::array<std::byte, memkit::SpscQueue<frame_event>::storage_bytes<8>()> event_backing{};
    memkit::SpscQueue<frame_event> events;
    assert(memkit::ok(events.init(event_backing.data(), 8u)));

    memkit::TokenBucket tx_budget;
    assert(memkit::ok(tx_budget.init(4u, 2u)));

    const std::uint8_t uart_chunk[] = {0x01u, 0x02u, 0x03u, 0x11u, 0x22u, 0x33u};
    simulate_uart_rx(rx, uart_chunk, sizeof uart_chunk);
    parse_frames(rx, events);

    tx_budget.refill();
    std::size_t handled = 0u;
    frame_event event{};
    while (memkit::ok(events.pop(event)) && memkit::ok(tx_budget.try_consume())) {
        std::printf(
            "frame: %02X %02X %02X\n",
            event.bytes[0],
            event.bytes[1],
            event.bytes[2]
        );
        ++handled;
    }

    assert(handled == 2u);
    std::printf("comm_pipeline: handled %zu frames\n", handled);
    return 0;
}
