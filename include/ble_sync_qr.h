#ifndef BLE_SYNC_QR_H
#define BLE_SYNC_QR_H

#include <stdbool.h>
#include <stdint.h>

#define BLE_SYNC_QR_MODULES 37U

/*
 * Error-correction level Q QR code for:
 * https://anhtu0310.github.io/VietCalendarTimeSync/
 */
bool ble_sync_qr_module(uint8_t x, uint8_t y);

#endif
