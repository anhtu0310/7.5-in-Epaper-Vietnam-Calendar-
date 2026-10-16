#include "epd7in5b.h"
#include "hal.h"

#include <assert.h>
#include <stdio.h>

#define MAX_TRANSFERS (EPD7IN5B_FRAME_BYTES + 64U)

typedef struct {
    bool data;
    uint8_t value;
} transfer_t;

static transfer_t transfers[MAX_TRANSFERS];
static uint32_t transfer_count;
static bool dc_level;

void hal_init(void) {}
void hal_delay_ms(uint32_t milliseconds) { (void)milliseconds; }
void hal_log(const char *message) { (void)message; }
void hal_log_u32(const char *prefix, uint32_t value) { (void)prefix; (void)value; }
void hal_idle(void) {}

void hal_gpio_write(uint32_t pin, bool high) {
    if (pin == EPD_DC_PIN) dc_level = high;
}

bool hal_gpio_read(uint32_t pin) {
    (void)pin;
    return true; /* BUSY released. */
}

bool hal_spi_write(uint8_t value) {
    assert(transfer_count < MAX_TRANSFERS);
    transfers[transfer_count].data = dc_level;
    transfers[transfer_count].value = value;
    ++transfer_count;
    return true;
}

static void reset_capture(void) {
    transfer_count = 0U;
    dc_level = false;
}

static void expect(uint32_t index, bool data, uint8_t value) {
    assert(index < transfer_count);
    assert(transfers[index].data == data);
    assert(transfers[index].value == value);
}

static void test_init_sequence(void) {
    static const transfer_t expected[] = {
        {false, 0x01}, {true, 0x37}, {true, 0x00},
        {false, 0x00}, {true, 0xCF}, {true, 0x08},
        {false, 0x30}, {true, 0x3A},
        {false, 0x82}, {true, 0x28},
        {false, 0x06}, {true, 0xC7}, {true, 0xCC}, {true, 0x15},
        {false, 0x50}, {true, 0x77},
        {false, 0x60}, {true, 0x22},
        {false, 0x65}, {true, 0x00},
        {false, 0x61}, {true, 0x02}, {true, 0x80}, {true, 0x01}, {true, 0x80},
        {false, 0xE5}, {true, 0x03},
    };

    reset_capture();
    assert(epd7in5b_init());
    assert(transfer_count == (sizeof(expected) / sizeof(expected[0])));
    for (uint32_t i = 0U; i < transfer_count; ++i) {
        assert(transfers[i].data == expected[i].data);
        assert(transfers[i].value == expected[i].value);
    }
}

static void test_pattern_stream(void) {
    const uint32_t row_bytes = EPD7IN5B_WIDTH / 2U;
    const uint32_t top_bytes = (EPD7IN5B_HEIGHT / 3U) * row_bytes;
    const uint32_t middle_bytes = (EPD7IN5B_HEIGHT / 3U) * row_bytes;

    reset_capture();
    assert(epd7in5b_display_test_pattern());
    assert(transfer_count == EPD7IN5B_FRAME_BYTES + 5U);
    expect(0U, false, 0x10U);
    expect(1U, true, 0x00U);
    expect(top_bytes, true, 0x00U);
    expect(1U + top_bytes, true, 0x44U);
    expect(top_bytes + middle_bytes, true, 0x44U);
    expect(1U + top_bytes + middle_bytes, true, 0x33U);
    expect(transfer_count - 5U, true, 0x33U);
    expect(transfer_count - 4U, false, 0x04U);
    expect(transfer_count - 3U, false, 0x71U);
    expect(transfer_count - 2U, false, 0x12U);
    expect(transfer_count - 1U, false, 0x71U);
}

static epd_color_t alternating_pixels(uint16_t x, uint16_t y, void *context) {
    (void)y;
    (void)context;
    return ((x & 1U) == 0U) ? EPD_COLOR_BLACK : EPD_COLOR_RED;
}

static void test_generated_stream(void) {
    reset_capture();
    assert(epd7in5b_display_generated(alternating_pixels, 0));
    assert(transfer_count == EPD7IN5B_FRAME_BYTES + 5U);
    expect(0U, false, 0x10U);
    expect(1U, true, 0x04U);
    expect(EPD7IN5B_FRAME_BYTES, true, 0x04U);
    expect(transfer_count - 4U, false, 0x04U);
    expect(transfer_count - 3U, false, 0x71U);
    expect(transfer_count - 2U, false, 0x12U);
    expect(transfer_count - 1U, false, 0x71U);
}

int main(void) {
    test_init_sequence();
    test_pattern_stream();
    test_generated_stream();
    puts("protocol tests passed");
    return 0;
}
