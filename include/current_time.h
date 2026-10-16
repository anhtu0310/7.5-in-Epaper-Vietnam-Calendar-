#ifndef CURRENT_TIME_H
#define CURRENT_TIME_H

#include <stdbool.h>
#include <stdint.h>

#include "calendar.h"

#define CTS_CURRENT_TIME_LENGTH 10U

/* Bluetooth SIG Current Time characteristic (0x2A2B), in local Vietnam time. */
bool current_time_decode(const uint8_t *value, uint16_t length,
                         calendar_date_t *date, uint32_t *seconds_until_midnight);
void current_time_encode(const calendar_date_t *date, uint32_t seconds_until_midnight,
                         uint8_t value[CTS_CURRENT_TIME_LENGTH]);

#endif
