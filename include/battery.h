#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BATTERY_OK = 0,
    BATTERY_LOW,
    BATTERY_CRITICAL
} battery_state_t;

bool battery_measure_mv(uint16_t *millivolts);
battery_state_t battery_classify(uint16_t millivolts, bool was_critical);

#endif
