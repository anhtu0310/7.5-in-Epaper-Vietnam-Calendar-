#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "calendar.h"
#include "calendar_render.h"
#include "ble_sync_qr.h"
#include "font_digits.h"
#include "font_vietnamese.h"
#include "lunar.h"
#include "special_days.h"
#include "vietnamese_canchi.h"

static epd_color_t portrait_pixel(uint16_t x, uint16_t y) {
#if !defined(CALENDAR_ROTATION) || CALENDAR_ROTATION == 90
    return calendar_render_pixel(y, (uint16_t)(383U - x), 0);
#else
    return calendar_render_pixel((uint16_t)(639U - y), x, 0);
#endif
}

int main(void) {
    char runtime_source[] = "nRF51";
    char runtime_copy[sizeof(runtime_source)];
    memset(runtime_copy, 0x55, sizeof(runtime_copy));
    memcpy(runtime_copy, runtime_source, sizeof(runtime_source));
    assert(strcmp(runtime_copy, runtime_source) == 0);

    calendar_date_t date = {2026U, 8U, 31U};
    lunar_date_t lunar;
    assert(calendar_date_valid(&date));
    assert(calendar_weekday(&date) == 1U);

    canchi_name_t canchi = vietnamese_canchi_day(&date);
    assert(strcmp(canchi.stem, "Đinh") == 0);
    assert(strcmp(canchi.branch, "Sửu") == 0);

    assert(lunar_from_solar(&date, &lunar));
    canchi = vietnamese_canchi_month(&lunar);
    assert(strcmp(canchi.stem, "Bính") == 0);
    assert(strcmp(canchi.branch, "Thân") == 0);
    canchi = vietnamese_canchi_year(&lunar);
    assert(strcmp(canchi.stem, "Bính") == 0);
    assert(strcmp(canchi.branch, "Ngọ") == 0);

    special_day_match_t upcoming = special_day_next(&date, 4U);
    assert(upcoming.days_away == 2U);
    assert(strcmp(upcoming.name, "QUỐC KHÁNH") == 0);

    calendar_date_t tet = {2026U, 2U, 17U};
    assert(lunar_from_solar(&tet, &lunar));
    assert(lunar.year == 2026U);
    assert(lunar.month == 1U);
    assert(lunar.day == 1U);
    assert(!lunar.leap_month);
    assert(strcmp(special_day_on(&tet), "TẾT NGUYÊN ĐÁN") == 0);

    /* Known Vietnamese/Chinese divergence dates from Hồ Ngọc Đức's rules. */
    calendar_date_t tet_2007 = {2007U, 2U, 17U};
    assert(lunar_from_solar(&tet_2007, &lunar));
    assert(lunar.year == 2007U && lunar.month == 1U && lunar.day == 1U && !lunar.leap_month);
    calendar_add_days(&tet_2007, 1U);
    assert(lunar_from_solar(&tet_2007, &lunar));
    assert(lunar.month == 1U && lunar.day == 2U);

    calendar_date_t tet_2030 = {2030U, 2U, 2U};
    assert(lunar_from_solar(&tet_2030, &lunar));
    assert(lunar.year == 2030U && lunar.month == 1U && lunar.day == 1U && !lunar.leap_month);

    calendar_date_t leap_2004 = {2004U, 3U, 21U};
    assert(lunar_from_solar(&leap_2004, &lunar));
    assert(lunar.year == 2004U && lunar.month == 2U && lunar.day == 1U && lunar.leap_month);

    const char *utf8 = "ỊỆƯĐ";
    assert(font_vi_utf8_next(&utf8) == 0x1ECAU);
    assert(font_vi_utf8_next(&utf8) == 0x1EC6U);
    assert(font_vi_utf8_next(&utf8) == 0x01AFU);
    assert(font_vi_utf8_next(&utf8) == 0x0110U);
    assert(*utf8 == '\0');
    assert(font_vi_find(0x01AFU)->codepoint == 0x01AFU); /* Ư */
    assert(font_vi_find(0x1EC6U)->codepoint == 0x1EC6U); /* Ệ */
    assert(font_vi_find(0x0110U)->codepoint == 0x0110U); /* Đ */
    assert(FONT_VI_HEIGHT == 25U);
    assert(font_vi_text_width("LỊCH VIỆT NAM") > 0U);
    assert(font_digits_text_width("31", FONT_DIGITS_LARGE) > 120U);
    assert(font_digits_text_width("19", FONT_DIGITS_MEDIUM) <
           font_digits_text_width("19", FONT_DIGITS_LARGE));
    assert(font_digits_find(FONT_DIGITS_LARGE, '8') != 0);

    /* Fixed version-5 QR code retains its finder patterns and quiet rendering area. */
    assert(BLE_SYNC_QR_MODULES == 37U);
    assert(ble_sync_qr_module(0U, 0U));
    assert(!ble_sync_qr_module(1U, 1U));
    assert(ble_sync_qr_module(3U, 3U));
    assert(!ble_sync_qr_module(BLE_SYNC_QR_MODULES, 0U));

    calendar_date_t rollover = {2028U, 2U, 28U};
    calendar_add_days(&rollover, 2U);
    assert(rollover.year == 2028U && rollover.month == 3U && rollover.day == 1U);

    assert(calendar_render_prepare(&date));
    assert(portrait_pixel(4U, 4U) == EPD_COLOR_RED);
    assert(portrait_pixel(6U, 6U) == EPD_COLOR_WHITE);
    /* 31 August 2026 is Monday: cell 36, red outline around today's cell. */
    assert(portrait_pixel(62U, 474U) == EPD_COLOR_RED);
    /* Lễ Vu Lan is lunar 15/7 (27 August in 2026): minor red dot. */
    assert(portrait_pixel(264U, 443U) == EPD_COLOR_RED);

    assert(calendar_render_prepare_ble_sync());
    /* QR finder pattern starts after the four-module white quiet zone. */
    assert(portrait_pixel(81U, 119U) == EPD_COLOR_BLACK);
    assert(portrait_pixel(57U, 95U) == EPD_COLOR_WHITE);

    calendar_date_t national_day = {2026U, 9U, 2U};
    special_day_match_t national_match = special_day_match_on(&national_day);
    assert(national_match.importance == SPECIAL_MINOR);
    assert(calendar_render_prepare(&national_day));
    /* 2 September 2026 is Wednesday: latest UI marks it with a red dot. */
    assert(portrait_pixel(212U, 287U) == EPD_COLOR_RED);

    for (uint16_t year = 2000U; year <= 2050U; ++year) {
        for (uint8_t month = 1U; month <= 12U; ++month) {
            uint8_t last = calendar_days_in_month(year, month);
            for (uint8_t day = 1U; day <= last; ++day) {
                calendar_date_t every_date = {year, month, day};
                assert(lunar_from_solar(&every_date, &lunar));
                assert(lunar.month >= 1U && lunar.month <= 12U);
                assert(lunar.day >= 1U && lunar.day <= 30U);
            }
        }
    }

    puts("calendar tests passed");
    return 0;
}
