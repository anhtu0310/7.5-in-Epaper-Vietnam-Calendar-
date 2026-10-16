#include "current_time.h"

bool current_time_decode(const uint8_t *value, uint16_t length,
                         calendar_date_t *date, uint32_t *seconds_until_midnight) {
    if ((value == 0) || (date == 0) || (seconds_until_midnight == 0) ||
        (length != CTS_CURRENT_TIME_LENGTH)) return false;

    calendar_date_t decoded = {
        (uint16_t)value[0] | ((uint16_t)value[1] << 8U),
        value[2],
        value[3]
    };
    uint8_t hour = value[4];
    uint8_t minute = value[5];
    uint8_t second = value[6];
    uint8_t day_of_week = value[7];
    uint8_t adjust_reason = value[9];

    if (!calendar_date_valid(&decoded) || (hour > 23U) || (minute > 59U) ||
        (second > 59U) || (day_of_week > 7U) || ((adjust_reason & 0xF0U) != 0U))
        return false;

    uint32_t elapsed = (uint32_t)hour * 3600UL +
                       (uint32_t)minute * 60UL + (uint32_t)second;
    *date = decoded;
    *seconds_until_midnight = 86400UL - elapsed;
    return true;
}

void current_time_encode(const calendar_date_t *date, uint32_t seconds_until_midnight,
                         uint8_t value[CTS_CURRENT_TIME_LENGTH]) {
    uint32_t elapsed = 0U;
    if ((seconds_until_midnight > 0U) && (seconds_until_midnight <= 86400UL))
        elapsed = 86400UL - seconds_until_midnight;

    uint8_t weekday = calendar_weekday(date); /* Calendar uses Sunday = 0. */
    value[0] = (uint8_t)(date->year & 0xFFU);
    value[1] = (uint8_t)(date->year >> 8U);
    value[2] = date->month;
    value[3] = date->day;
    value[4] = (uint8_t)(elapsed / 3600UL);
    value[5] = (uint8_t)((elapsed / 60UL) % 60UL);
    value[6] = (uint8_t)(elapsed % 60UL);
    value[7] = (weekday == 0U) ? 7U : weekday; /* CTS: Monday = 1, Sunday = 7. */
    value[8] = 0U; /* Fractions256 are not tracked by the calendar. */
    value[9] = 0U; /* Adjustment reason is unknown for the initial value. */
}
