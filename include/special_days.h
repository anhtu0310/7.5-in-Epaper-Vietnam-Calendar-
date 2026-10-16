#ifndef SPECIAL_DAYS_H
#define SPECIAL_DAYS_H

#include <stdint.h>

#include "calendar.h"

typedef struct {
    const char *name;
    uint8_t days_away;
    uint8_t event_month;
    uint8_t event_day;
    calendar_date_t solar_date;
    enum {
        SPECIAL_NONE = 0,
        SPECIAL_MINOR = 1,
        SPECIAL_MAJOR = 2,
    } importance;
    enum {
        SPECIAL_SOLAR = 0,
        SPECIAL_LUNAR = 1,
    } calendar;
} special_day_match_t;

const char *special_day_on(const calendar_date_t *date);
special_day_match_t special_day_match_on(const calendar_date_t *date);
special_day_match_t special_day_next(const calendar_date_t *today, uint8_t lookahead_days);
uint8_t special_days_upcoming(const calendar_date_t *today, uint8_t lookahead_days,
                              special_day_match_t *matches, uint8_t capacity);

#endif
