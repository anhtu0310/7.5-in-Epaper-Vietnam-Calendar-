#include <assert.h>
#include <stdio.h>

#include "battery.h"
#include "current_time.h"
#include "unix_time.h"

int main(void) {
    calendar_date_t date;
    uint32_t remaining;

    assert(unix_time_to_vietnam(1788195600UL, &date, &remaining));
    assert(date.year == 2026U && date.month == 9U && date.day == 1U);
    assert(remaining == 86400UL);

    assert(unix_time_to_vietnam(1788240896UL, &date, &remaining));
    assert(date.year == 2026U && date.month == 9U && date.day == 1U);
    assert(remaining == 41104UL);

    const uint8_t cts_value[CTS_CURRENT_TIME_LENGTH] = {
        0xEAU, 0x07U, 9U, 1U, 18U, 30U, 15U, 2U, 0U, 1U
    };
    assert(current_time_decode(cts_value, sizeof(cts_value), &date, &remaining));
    assert(date.year == 2026U && date.month == 9U && date.day == 1U);
    assert(remaining == 19785UL);

    uint8_t encoded[CTS_CURRENT_TIME_LENGTH];
    current_time_encode(&date, remaining, encoded);
    for (uint8_t i = 0U; i < 9U; ++i) assert(encoded[i] == cts_value[i]);
    assert(encoded[9] == 0U);

    uint8_t invalid[CTS_CURRENT_TIME_LENGTH] = {
        0xEAU, 0x07U, 9U, 1U, 24U, 0U, 0U, 2U, 0U, 0U
    };
    assert(!current_time_decode(invalid, sizeof(invalid), &date, &remaining));

    assert(battery_classify(3000U, false) == BATTERY_OK);
    assert(battery_classify(2300U, false) == BATTERY_LOW);
    assert(battery_classify(2100U, false) == BATTERY_CRITICAL);
    assert(battery_classify(2150U, true) == BATTERY_CRITICAL);
    assert(battery_classify(2200U, true) == BATTERY_LOW);

    puts("power/time tests passed");
    return 0;
}
