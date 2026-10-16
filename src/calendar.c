#include "calendar.h"

bool calendar_is_leap_year(uint16_t year) {
    return ((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U));
}

uint8_t calendar_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t lengths[12] = {31U, 28U, 31U, 30U, 31U, 30U,
                                        31U, 31U, 30U, 31U, 30U, 31U};
    if ((month < 1U) || (month > 12U)) return 0U;
    if ((month == 2U) && calendar_is_leap_year(year)) return 29U;
    return lengths[month - 1U];
}

bool calendar_date_valid(const calendar_date_t *date) {
    if ((date == 0) || (date->year < 2000U) || (date->year > 2050U)) return false;
    uint8_t days = calendar_days_in_month(date->year, date->month);
    return (days != 0U) && (date->day >= 1U) && (date->day <= days);
}

uint8_t calendar_weekday(const calendar_date_t *date) {
    /* Tomohiko Sakamoto's Gregorian calendar algorithm. */
    static const uint8_t offsets[12] = {0U, 3U, 2U, 5U, 0U, 3U,
                                        5U, 1U, 4U, 6U, 2U, 4U};
    uint16_t year = date->year;
    if (date->month < 3U) --year;
    return (uint8_t)((year + year / 4U - year / 100U + year / 400U +
                      offsets[date->month - 1U] + date->day) % 7U);
}

uint32_t calendar_julian_day(const calendar_date_t *date) {
    uint32_t a = (14U - date->month) / 12U;
    uint32_t y = (uint32_t)date->year + 4800U - a;
    uint32_t m = (uint32_t)date->month + 12U * a - 3U;
    return (uint32_t)date->day + (153U * m + 2U) / 5U + 365U * y + y / 4U -
           y / 100U + y / 400U - 32045U;
}

void calendar_add_days(calendar_date_t *date, uint16_t days) {
    while (days-- != 0U) {
        uint8_t last = calendar_days_in_month(date->year, date->month);
        if (date->day < last) {
            ++date->day;
        } else {
            date->day = 1U;
            if (date->month < 12U) {
                ++date->month;
            } else {
                date->month = 1U;
                ++date->year;
            }
        }
    }
}
