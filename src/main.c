#include "epd7in5b.h"
#include "battery.h"
#include "ble_time_sync.h"
#include "button.h"
#include "calendar.h"
#include "calendar_render.h"
#include "flash_date.h"
#include "hal.h"
#include "time_source.h"

#ifndef CALENDAR_START_YEAR
#define CALENDAR_START_YEAR 2026
#endif
#ifndef CALENDAR_START_MONTH
#define CALENDAR_START_MONTH 8
#endif
#ifndef CALENDAR_START_DAY
#define CALENDAR_START_DAY 31
#endif
#ifndef WAKE_INTERVAL_SECONDS
#define WAKE_INTERVAL_SECONDS 86400UL
#endif
#ifndef BLE_ENABLE
#define BLE_ENABLE 0
#endif

static void stop(const char *reason) {
    hal_epd_power_down();
    hal_log("FAILED: ");
    hal_log(reason);
    hal_log("\n");
    for (;;) hal_idle();
}

static bool display_prepared_frame(void) {
    bool success = false;
    hal_init();

    if (epd7in5b_init() &&
        epd7in5b_display_generated(calendar_render_pixel, 0) &&
        epd7in5b_sleep()) {
        success = true;
    }

    /* Always release SPI, BUSY and the high-frequency clock on every exit path. */
    hal_epd_power_down();
    return success;
}

#if BLE_ENABLE
static bool display_ble_sync_screen(void) {
    hal_log("Showing BLE QR synchronization screen\n");
    return calendar_render_prepare_ble_sync() && display_prepared_frame();
}
#endif

static void log_banner(void) {
    hal_log("Vietnamese calendar - Waveshare epd7in5b nRF51822 port\n");
    hal_log_u32("MOSI pin: ", EPD_MOSI_PIN);
    hal_log_u32("SCLK pin: ", EPD_SCLK_PIN);
    hal_log_u32("CS pin: ", EPD_CS_PIN);
    hal_log_u32("DC pin: ", EPD_DC_PIN);
    hal_log_u32("RST pin: ", EPD_RST_PIN);
    hal_log_u32("BUSY pin: ", EPD_BUSY_PIN);
    hal_log_u32("BS pin: ", EPD_BS_PIN);
#if EPD_USE_HARDWARE_SPI
    hal_log("SPI backend: hardware SPI0, 2 MHz\n");
#else
    hal_log("SPI backend: software mode 0, MSB first\n");
#endif
#if BLE_ENABLE
    hal_log("BLE: S130 enabled on startup and by 3-second button hold\n");
#else
    hal_log("BLE: unavailable in this build\n");
#endif
}

int main(void) {
    hal_init();
    log_banner();
    /* Do not leave SPI0 and the panel interface powered during BLE setup. */
    hal_epd_power_down();

    calendar_date_t today = {CALENDAR_START_YEAR, CALENDAR_START_MONTH, CALENDAR_START_DAY};
    if (!calendar_date_valid(&today)) stop("invalid CALENDAR_START date");
    if (flash_date_load(&today)) hal_log("Loaded last displayed date from flash\n");
    else hal_log("No saved date; using build-time fallback\n");

    uint32_t first_interval = WAKE_INTERVAL_SECONDS;
#if BLE_ENABLE
    uint16_t startup_battery_mv = 0U;
    bool startup_battery_valid = battery_measure_mv(&startup_battery_mv);
    bool startup_battery_critical = startup_battery_valid &&
        battery_classify(startup_battery_mv, false) == BATTERY_CRITICAL;
    if (startup_battery_critical) {
        hal_log("Critical battery at startup: BLE skipped\n");
    } else {
        if (!display_ble_sync_screen())
            hal_log("Warning: BLE QR screen refresh failed\n");
        if (ble_time_sync_run(&today, &first_interval))
            hal_log("Startup BLE time sync succeeded\n");
    }
#endif

    button_init();
    if (!time_source_init(first_interval)) stop("RTC0 initialization");

    bool refresh_pending = true;
    bool critical_latched = false;
    bool critical_screen_shown = false;

    for (;;) {
        if (refresh_pending) {
            uint16_t battery_mv = 0U;
            bool battery_valid = battery_measure_mv(&battery_mv);
            battery_state_t battery_state = battery_valid ?
                battery_classify(battery_mv, critical_latched) : BATTERY_OK;
            critical_latched = battery_state == BATTERY_CRITICAL;

            if (battery_valid) hal_log_u32("Battery mV: ", battery_mv);
            else hal_log("Warning: battery ADC timed out\n");

            if (battery_state == BATTERY_CRITICAL) {
                if (!critical_screen_shown) {
                    hal_log("Critical battery: showing final warning frame\n");
                    if (!calendar_render_prepare_low_battery(battery_mv))
                        stop("low-battery frame preparation");
                    if (!display_prepared_frame()) stop("low-battery frame refresh");
                    critical_screen_shown = true;
                } else {
                    hal_log("Critical battery: e-paper refresh suppressed; RTC continues\n");
                }
            } else {
                critical_screen_shown = false;
                if (battery_valid)
                    calendar_render_set_battery(battery_mv, battery_state == BATTERY_LOW);
                hal_log("Calendar refresh begin\n");
                hal_log_u32("Year: ", today.year);
                hal_log_u32("Month: ", today.month);
                hal_log_u32("Day: ", today.day);
                if (!calendar_render_prepare(&today)) stop("calendar preparation");
                if (!display_prepared_frame()) stop("calendar frame or refresh");
                hal_log("Calendar refresh complete; panel asleep\n");
                if (!flash_date_save(&today)) hal_log("Warning: date flash save failed\n");
            }
            refresh_pending = false;
        }

        time_source_event_t event = time_source_wait_event();
        if (event == TIME_SOURCE_DAY) {
            calendar_add_days(&today, 1U);
            if (!time_source_schedule(WAKE_INTERVAL_SECONDS)) stop("daily RTC schedule");
            refresh_pending = true;
            continue;
        }

        uint32_t remaining_seconds = time_source_pause();
        bool long_press = button_wait_for_long_press();
        if (long_press) {
#if BLE_ENABLE
            uint32_t until_midnight = remaining_seconds;
            if (critical_latched) {
                hal_log("Critical battery: BLE sync refused\n");
                if (!time_source_schedule(remaining_seconds)) stop("RTC resume after low battery");
            } else {
                bool qr_screen_shown = display_ble_sync_screen();
                if (!qr_screen_shown) hal_log("Warning: BLE QR screen refresh failed\n");
                if (ble_time_sync_run(&today, &until_midnight)) {
                    if (!time_source_schedule(until_midnight)) stop("synced RTC schedule");
                    /* The successful calendar refresh below performs the single flash save. */
                    refresh_pending = true;
                } else {
                    if (!time_source_schedule(remaining_seconds))
                        stop("RTC resume after BLE timeout");
                    /* Restore the calendar after a QR screen whose BLE window timed out. */
                    if (qr_screen_shown) refresh_pending = true;
                }
            }
#else
            hal_log("Button held, but BLE is not available on this build\n");
            if (!time_source_schedule(remaining_seconds)) stop("RTC resume after button");
#endif
        } else if (!time_source_schedule(remaining_seconds)) {
            stop("RTC resume after short press");
        }
        button_rearm_when_released();
    }
}
