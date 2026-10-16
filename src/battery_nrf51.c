#include "battery.h"

#include <stdint.h>

#ifndef BATTERY_LOW_MV
#define BATTERY_LOW_MV 2300U
#endif
#ifndef BATTERY_CRITICAL_MV
#define BATTERY_CRITICAL_MV 2100U
#endif
#ifndef BATTERY_RECOVERY_MV
#define BATTERY_RECOVERY_MV 100U
#endif
#ifndef BATTERY_CAL_NUMERATOR_MV
/* VDD/3 against the nominal 1.2 V band-gap, 10-bit ADC. */
#define BATTERY_CAL_NUMERATOR_MV 3600UL
#endif

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define ADC_TASKS_START REG32(0x40007000UL)
#define ADC_TASKS_STOP  REG32(0x40007004UL)
#define ADC_EVENTS_END  REG32(0x40007100UL)
#define ADC_ENABLE      REG32(0x40007500UL)
#define ADC_CONFIG      REG32(0x40007504UL)
#define ADC_RESULT      REG32(0x40007508UL)

#define ADC_ENABLE_ENABLED 1UL
#define ADC_RES_10BIT       2UL
#define ADC_INPSEL_VDD_1_3  (6UL << 2U)
#define ADC_TIMEOUT         1000000UL

bool battery_measure_mv(uint16_t *millivolts) {
    if (millivolts == 0) return false;

    ADC_ENABLE = ADC_ENABLE_ENABLED;
    ADC_CONFIG = ADC_RES_10BIT | ADC_INPSEL_VDD_1_3;
    ADC_EVENTS_END = 0U;
    ADC_TASKS_START = 1U;

    uint32_t timeout = ADC_TIMEOUT;
    while ((ADC_EVENTS_END == 0U) && (timeout != 0U)) --timeout;
    if (timeout == 0U) {
        ADC_TASKS_STOP = 1U;
        ADC_ENABLE = 0U;
        return false;
    }

    uint32_t sample = ADC_RESULT & 0x3FFUL;
    ADC_TASKS_STOP = 1U;
    ADC_EVENTS_END = 0U;
    ADC_ENABLE = 0U;
    *millivolts = (uint16_t)((sample * BATTERY_CAL_NUMERATOR_MV + 511UL) / 1023UL);
    return true;
}

battery_state_t battery_classify(uint16_t millivolts, bool was_critical) {
    if (was_critical && millivolts < (BATTERY_CRITICAL_MV + BATTERY_RECOVERY_MV))
        return BATTERY_CRITICAL;
    if (millivolts <= BATTERY_CRITICAL_MV) return BATTERY_CRITICAL;
    if (millivolts <= BATTERY_LOW_MV) return BATTERY_LOW;
    return BATTERY_OK;
}
