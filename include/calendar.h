#ifndef CALENDAR_H
#define CALENDAR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
} calendar_date_t;

bool calendar_date_valid(const calendar_date_t *date);
bool calendar_is_leap_year(uint16_t year);
uint8_t calendar_days_in_month(uint16_t year, uint8_t month);
uint8_t calendar_weekday(const calendar_date_t *date); /* Sunday = 0. */
uint32_t calendar_julian_day(const calendar_date_t *date);
void calendar_add_days(calendar_date_t *date, uint16_t days);

#endif
