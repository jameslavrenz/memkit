/*
 * Embedded utility patterns: DMA ping-pong, multi-producer queue, calibration.
 */

#include <cassert>
#include <cstdint>
#include <cstdio>

#include <memkit/memkit.hpp>

struct adc_block {
    std::uint16_t samples[4];
};

struct isr_event {
    std::uint8_t source_id;
    std::uint16_t value;
};

static const std::int32_t adc_keys[]   = {0, 512, 1023};
static const float adc_volts[]         = {0.0f, 1.65f, 3.30f};

int main()
{
    /* --- DMA / ADC ping-pong (DoubleBuffer) --- */
    memkit::stl::array<std::byte, memkit::DoubleBuffer<adc_block>::storage_bytes<1>()> dma_backing{};
    memkit::DoubleBuffer<adc_block> adc_dma;
    assert(memkit::ok(adc_dma.init(dma_backing.data(), 1u)));

    memkit::stl::span<adc_block> producer = adc_dma.write_span();
    for (std::size_t i = 0u; i < 4u; ++i) {
        producer[0].samples[i] = static_cast<std::uint16_t>(100u + i);
    }
    adc_dma.publish();

    memkit::stl::span<const adc_block> consumer = adc_dma.read_span();
    std::printf(
        "dma ping-pong: published sample[0]=%u\n",
        static_cast<unsigned>(consumer[0].samples[0])
    );

    /* --- Multi-ISR → main loop (MpscQueue) --- */
    alignas(std::max_align_t) memkit::stl::array<
        std::byte,
        memkit::MpscQueue<isr_event>::storage_bytes<8>()
    > event_backing{};

    memkit::MpscQueue<isr_event> events;
    assert(memkit::ok(events.init(event_backing.data(), 8u)));

    const isr_event from_uart{1u, 0x55u};
    const isr_event from_spi{2u, 0xAAu};
    assert(memkit::ok(events.push(from_uart)));
    assert(memkit::ok(events.push(from_spi)));

    isr_event handled{};
    assert(memkit::ok(events.pop(handled)));
    assert(handled.source_id == 1u);
    assert(memkit::ok(events.pop(handled)));
    assert(handled.source_id == 2u);
    std::printf("mpsc queue: handled %u then %u\n", 1u, 2u);

    /* --- Sensor calibration (LookupTable) --- */
    memkit::LookupTable<std::int32_t, float> adc_curve;
    assert(memkit::ok(adc_curve.init(adc_keys, adc_volts, 3u)));
    const float volts_at_768 = adc_curve.at(768);
    std::printf("calibration: adc=768 -> %.2f V\n", static_cast<double>(volts_at_768));

    /* --- Bit-packed field decode (BitReader) --- */
    const std::byte frame[] = {std::byte{0xA5}, std::byte{0x3C}};
    memkit::BitReader bits;
    assert(memkit::ok(bits.init(frame)));
    std::uint32_t header = 0u;
    assert(memkit::ok(bits.read_bits(8u, header)));
    assert(header == 0xA5u);
    std::uint32_t flags = 0u;
    assert(memkit::ok(bits.read_bits(4u, flags)));
    std::printf("bit stream: header=0x%02X flags=0x%X\n", header, flags);

    /* --- Sensor smoothing (MovingAverage) --- */
    memkit::MovingAverage<std::int32_t, 4> filter;
    assert(memkit::ok(filter.push(10)));
    assert(memkit::ok(filter.push(20)));
    assert(memkit::ok(filter.push(30)));
    assert(filter.average() == 20);
    std::printf("moving average: last-3 avg=%ld\n", static_cast<long>(filter.average()));

    std::printf("embedded_patterns: ok\n");
    return 0;
}
