#include "unix_time.h"

#define VIETNAM_UTC_OFFSET_SECONDS (7UL * 3600UL)
#define SECONDS_PER_DAY 86400UL

bool unix_time_to_vietnam(uint32_t unix_utc, calendar_date_t *date,
                          uint32_t *seconds_until_midnight) {
    if ((date == 0) || (seconds_until_midnight == 0)) return false;
    uint64_t local = (uint64_t)unix_utc + VIETNAM_UTC_OFFSET_SECONDS;
    uint32_t days = (uint32_t)(local / SECONDS_PER_DAY);
    uint32_t second_of_day = (uint32_t)(local % SECONDS_PER_DAY);

    calendar_date_t result = {1970U, 1U, 1U};
    while (days >= (calendar_is_leap_year(result.year) ? 366UL : 365UL)) {
        days -= calendar_is_leap_year(result.year) ? 366UL : 365UL;
        ++result.year;
        if (result.year > 2099U) return false;
    }
    while (days >= calendar_days_in_month(result.year, result.month)) {
        days -= calendar_days_in_month(result.year, result.month);
        ++result.month;
    }
    result.day = (uint8_t)(days + 1U);
    *date = result;
    *seconds_until_midnight = SECONDS_PER_DAY - second_of_day;
    return true;
}
