#ifndef BLE_TIME_SYNC_H
#define BLE_TIME_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "calendar.h"

/* Advertises the Bluetooth SIG Current Time Service and accepts local VN time. */
bool ble_time_sync_run(calendar_date_t *date, uint32_t *seconds_until_midnight);

#endif
