#ifndef UNIX_TIME_H
#define UNIX_TIME_H

#include <stdbool.h>
#include <stdint.h>

#include "calendar.h"

bool unix_time_to_vietnam(uint32_t unix_utc, calendar_date_t *date,
                          uint32_t *seconds_until_midnight);

#endif
