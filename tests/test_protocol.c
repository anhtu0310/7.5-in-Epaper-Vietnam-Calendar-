#include "epd7in5b.h"
#include "hal.h"

#include <assert.h>
#include <stdio.h>

#define MAX_TRANSFERS (EPD7IN5B_FRAME_BYTES + 64U)

typedef struct {
    bool data;
    uint8_t value;
    bool panel_flash; /* Sent to the panel's SPI flash rather than the controller. */
} transfer_t;

static transfer_t transfers[MAX_TRANSFERS];
static uint32_t transfer_count;
static bool dc_level;

void hal_init(void) {}
void hal_delay_ms(uint32_t milliseconds) { (void)milliseconds; }
void hal_log(const char *message) { (void)message; }
void hal_log_u32(const char *prefix, uint32_t value) { (void)prefix; (void)value; }
void hal_idle(void) {}
uint32_t hal_timestamp(void) { return 0U; }
uint32_t hal_elapsed_ms(uint32_t since) { (void)since; return 0U; }

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

bool hal_panel_flash_command(uint8_t command) {
    assert(transfer_count < MAX_TRANSFERS);
    transfers[transfer_count].data = false;
    transfers[transfer_count].value = command;
    transfers[transfer_count].panel_flash = true;
    ++transfer_count;
    return true;
}

static void reset_capture(void) {
    transfer_count = 0U;
    dc_level = false;
    for (uint32_t i = 0U; i < MAX_TRANSFERS; ++i) transfers[i].panel_flash = false;
}

static void expect(uint32_t index, bool data, uint8_t value) {
    assert(index < transfer_count);
    assert(transfers[index].data == data);
    assert(transfers[index].value == value);
    assert(!transfers[index].panel_flash);
}

static void expect_sequence(uint32_t start, const transfer_t *expected, uint32_t count) {
    assert(start + count <= transfer_count);
    for (uint32_t i = 0U; i < count; ++i) {
        assert(transfers[start + i].data == expected[i].data);
        assert(transfers[start + i].value == expected[i].value);
        assert(transfers[start + i].panel_flash == expected[i].panel_flash);
    }
}

#if EPD_PANEL_FLASH
/* FLASH_CONTROL pass-through on, one flash command, pass-through off, BUSY check. */
#define PANEL_FLASH_STEPS(command) \
    {false, 0x65, false}, {true, 0x01, false}, {false, (command), true}, \
    {false, 0x65, false}, {true, 0x00, false}, {false, 0x71, false}
#define PANEL_FLASH_STEP_COUNT 6U
#else
#define PANEL_FLASH_STEP_COUNT 0U
#endif

static void test_init_sequence(void) {
    static const transfer_t expected[] = {
#if EPD_PANEL_FLASH
        PANEL_FLASH_STEPS(0xAB),
#endif
        {false, 0x01, false}, {true, 0x37, false}, {true, 0x00, false},
        {false, 0x00, false}, {true, 0xCF, false}, {true, 0x08, false},
        {false, 0x30, false}, {true, 0x3A, false},
        {false, 0x82, false}, {true, 0x28, false},
        {false, 0x06, false}, {true, 0xC7, false}, {true, 0xCC, false}, {true, 0x15, false},
        {false, 0x50, false}, {true, 0x77, false},
        {false, 0x60, false}, {true, 0x22, false},
        {false, 0x65, false}, {true, 0x00, false},
        {false, 0x61, false}, {true, 0x02, false}, {true, 0x80, false}, {true, 0x01, false}, {true, 0x80, false},
        {false, 0xE5, false}, {true, 0x03, false},
    };

    reset_capture();
    assert(epd7in5b_init());
    assert(transfer_count == (sizeof(expected) / sizeof(expected[0])));
    expect_sequence(0U, expected, transfer_count);
}

static void test_sleep_sequence(void) {
    static const transfer_t expected[] = {
#if EPD_PANEL_FLASH
        PANEL_FLASH_STEPS(0xB9),
#endif
        {false, 0x02, false}, {false, 0x71, false}, {false, 0x07, false}, {true, 0xA5, false},
    };

    reset_capture();
    assert(epd7in5b_sleep());
    assert(transfer_count == PANEL_FLASH_STEP_COUNT + 4U);
    assert(transfer_count == (sizeof(expected) / sizeof(expected[0])));
    expect_sequence(0U, expected, transfer_count);
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
    test_sleep_sequence();
    test_pattern_stream();
    test_generated_stream();
    puts("protocol tests passed");
    return 0;
}
