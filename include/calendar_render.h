#ifndef CALENDAR_RENDER_H
#define CALENDAR_RENDER_H

#include <stdbool.h>

#include "calendar.h"
#include "epd7in5b.h"

bool calendar_render_prepare(const calendar_date_t *today);
void calendar_render_set_battery(uint16_t millivolts, bool low_warning);
bool calendar_render_prepare_low_battery(uint16_t millivolts);
bool calendar_render_prepare_ble_sync(void);
epd_color_t calendar_render_pixel(uint16_t x, uint16_t y, void *context);

#endif
