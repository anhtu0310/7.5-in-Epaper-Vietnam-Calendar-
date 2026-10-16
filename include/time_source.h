#ifndef TIME_SOURCE_H
#define TIME_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TIME_SOURCE_DAY = 0,
    TIME_SOURCE_BUTTON
} time_source_event_t;

bool time_source_init(uint32_t first_interval_seconds);
bool time_source_schedule(uint32_t interval_seconds);
time_source_event_t time_source_wait_event(void);
uint32_t time_source_pause(void);

#endif
