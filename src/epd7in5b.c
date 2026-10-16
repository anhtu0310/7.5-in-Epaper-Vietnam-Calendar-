/*
 * Clean C port of Waveshare RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in5bc.c.
 * Source commit: a794fbc39656b0f93938d1ffb3fdc77eaed9e9fc
 *
 * The register sequence, reset timing, SPI mode/frequency, BUSY polarity and
 * native UC8159 pixel values follow the Waveshare implementation.
 */

#include "epd7in5b.h"

#include "hal.h"

#ifndef EPD_DC_PIN
#error "EPD_DC_PIN must be supplied by the Makefile"
#endif
#ifndef EPD_RST_PIN
#error "EPD_RST_PIN must be supplied by the Makefile"
#endif
#ifndef EPD_BUSY_PIN
#error "EPD_BUSY_PIN must be supplied by the Makefile"
#endif

#define PANEL_SETTING                  0x00U
#define POWER_SETTING                  0x01U
#define POWER_OFF                      0x02U
#define POWER_ON                       0x04U
#define BOOSTER_SOFT_START             0x06U
#define DEEP_SLEEP                     0x07U
#define DATA_START_TRANSMISSION_1      0x10U
#define DISPLAY_REFRESH                0x12U
#define PLL_CONTROL                    0x30U
#define TEMPERATURE_CALIBRATION        0x41U
#define VCOM_AND_DATA_INTERVAL_SETTING 0x50U
#define TCON_SETTING                   0x60U
#define TCON_RESOLUTION                0x61U
#define FLASH_CONTROL                  0x65U
#define GET_STATUS                     0x71U
#define VCM_DC_SETTING                 0x82U
#define FLASH_MODE                     0xE5U

#define BUSY_TIMEOUT_MS 60000U

static bool send_command(uint8_t command) {
    hal_gpio_write(EPD_DC_PIN, false);
    return hal_spi_write(command);
}

static bool send_data(uint8_t data) {
    hal_gpio_write(EPD_DC_PIN, true);
    return hal_spi_write(data);
}

static bool wait_until_idle(void) {
    uint32_t elapsed_ms = 0U;

    hal_log("EPD BUSY wait (active low)\n");
    for (;;) {
        /* Waveshare V3 requires GET_STATUS before every BUSY sample. */
        if (!send_command(GET_STATUS)) return false;
        if (hal_gpio_read(EPD_BUSY_PIN)) break;
        if (elapsed_ms >= BUSY_TIMEOUT_MS) {
            hal_log("EPD BUSY timeout\n");
            return false;
        }
        hal_delay_ms(100U);
        elapsed_ms += 100U;
    }
    hal_log_u32("EPD BUSY released after ms: ", elapsed_ms);
    return true;
}

static void reset_panel(void) {
    /* Exact high-low-high reset from Waveshare EPD_7in5bc.c V3. */
    hal_log("EPD reset HIGH\n");
    hal_gpio_write(EPD_RST_PIN, true);
    hal_delay_ms(200U);
    hal_log("EPD reset LOW\n");
    hal_gpio_write(EPD_RST_PIN, false);
    hal_delay_ms(2U);
    hal_log("EPD reset HIGH\n");
    hal_gpio_write(EPD_RST_PIN, true);
    hal_delay_ms(200U);
    hal_log("EPD reset complete\n");
}

static bool refresh(void) {
    hal_log("EPD refresh: POWER_ON\n");
    if (!send_command(POWER_ON) || !wait_until_idle()) return false;

    hal_log("EPD refresh: DISPLAY_REFRESH\n");
    if (!send_command(DISPLAY_REFRESH)) {
        return false;
    }
    hal_delay_ms(100U);
    return wait_until_idle();
}

bool epd7in5b_init(void) {
    reset_panel();

    hal_log("EPD init: POWER_SETTING\n");
    if (!send_command(POWER_SETTING) || !send_data(0x37U) || !send_data(0x00U)) return false;
    hal_log("EPD init: PANEL_SETTING\n");
    if (!send_command(PANEL_SETTING) || !send_data(0xCFU) || !send_data(0x08U)) return false;
    hal_log("EPD init: PLL_CONTROL\n");
    if (!send_command(PLL_CONTROL) || !send_data(0x3AU)) return false;
    hal_log("EPD init: VCM_DC_SETTING\n");
    if (!send_command(VCM_DC_SETTING) || !send_data(0x28U)) return false;
    hal_log("EPD init: BOOSTER_SOFT_START\n");
    if (!send_command(BOOSTER_SOFT_START) || !send_data(0xC7U) || !send_data(0xCCU) || !send_data(0x15U))
        return false;
    hal_log("EPD init: VCOM/TCON\n");
    if (!send_command(VCOM_AND_DATA_INTERVAL_SETTING) || !send_data(0x77U)) return false;
    if (!send_command(TCON_SETTING) || !send_data(0x22U)) return false;
    hal_log("EPD init: FLASH_CONTROL\n");
    if (!send_command(FLASH_CONTROL) || !send_data(0x00U)) return false;

    hal_log("EPD init: TCON_RESOLUTION\n");
    if (!send_command(TCON_RESOLUTION) || !send_data(0x02U) || !send_data(0x80U) || !send_data(0x01U) ||
        !send_data(0x80U))
        return false;

    hal_log("EPD init: FLASH_MODE\n");
    if (!send_command(FLASH_MODE) || !send_data(0x03U)) return false;

    hal_log("EPD init complete\n");
    return true;
}

bool epd7in5b_clean(void) {
    if (!send_command(DATA_START_TRANSMISSION_1)) return false;

    for (uint32_t i = 0U; i < EPD7IN5B_FRAME_BYTES; ++i) {
        if (!send_data(0x33U)) return false; /* Two white pixels. */
    }
    return refresh();
}

bool epd7in5b_display_test_pattern(void) {
    const uint32_t bytes_per_row = EPD7IN5B_WIDTH / 2U;

    if (!send_command(DATA_START_TRANSMISSION_1)) return false;

    for (uint32_t y = 0U; y < EPD7IN5B_HEIGHT; ++y) {
        uint8_t pixels;
        if (y < (EPD7IN5B_HEIGHT / 3U)) {
            pixels = 0x00U; /* two black pixels */
        } else if (y < ((EPD7IN5B_HEIGHT * 2U) / 3U)) {
            pixels = 0x44U; /* two red pixels */
        } else {
            pixels = 0x33U; /* two white pixels */
        }

        for (uint32_t x = 0U; x < bytes_per_row; ++x) {
            if (!send_data(pixels)) return false;
        }
    }

    return refresh();
}

bool epd7in5b_display_generated(epd_pixel_fn pixel, void *context) {
    if ((pixel == 0) || !send_command(DATA_START_TRANSMISSION_1)) return false;

    hal_log("EPD frame streaming begin (122880 bytes)\n");

    for (uint16_t y = 0U; y < EPD7IN5B_HEIGHT; ++y) {
        for (uint16_t x = 0U; x < EPD7IN5B_WIDTH; x += 2U) {
            uint8_t high = (uint8_t)pixel(x, y, context);
            uint8_t low = (uint8_t)pixel((uint16_t)(x + 1U), y, context);
            if (!send_data((uint8_t)((high << 4U) | low))) return false;
        }
    }
    hal_log("EPD frame streaming complete\n");
    return refresh();
}

bool epd7in5b_sleep(void) {
    if (!send_command(POWER_OFF) || !wait_until_idle()) return false;
    if (!send_command(DEEP_SLEEP)) return false;
    return send_data(0xA5U);
}
