#ifndef LUNAR_H
#define LUNAR_H

#include <stdbool.h>
#include <stdint.h>

#include "calendar.h"

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    bool leap_month;
} lunar_date_t;

/* Vietnamese UTC+7 lunisolar conversion, valid for solar years 2000..2050. */
bool lunar_from_solar(const calendar_date_t *solar, lunar_date_t *lunar);

#endif
