#include "time_source.h"

#include <stdint.h>

#include "button.h"
#include "hal.h"

#ifndef WAKE_INTERVAL_SECONDS
#define WAKE_INTERVAL_SECONDS 86400UL
#endif

#if !defined(LFCLK_SOURCE_rc) && !defined(LFCLK_SOURCE_xtal)
#error "LFCLK_SOURCE must be rc or xtal"
#endif

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define CLOCK_TASKS_LFCLKSTART  REG32(0x40000008UL)
#define CLOCK_EVENTS_LFCLKSTART REG32(0x40000104UL)
#define CLOCK_LFCLKSTAT          REG32(0x40000418UL)
#define CLOCK_LFCLKSRC          REG32(0x40000518UL)
#define POWER_TASKS_LOWPWR      REG32(0x4000007CUL)

#define RTC0_TASKS_START        REG32(0x4000B000UL)
#define RTC0_TASKS_STOP         REG32(0x4000B004UL)
#define RTC0_TASKS_CLEAR        REG32(0x4000B008UL)
#define RTC0_EVENTS_COMPARE0    REG32(0x4000B140UL)
#define RTC0_INTENSET           REG32(0x4000B304UL)
#define RTC0_INTENCLR           REG32(0x4000B308UL)
#define RTC0_COUNTER            REG32(0x4000B504UL)
#define RTC0_PRESCALER          REG32(0x4000B508UL)
#define RTC0_CC0                REG32(0x4000B540UL)

#define NVIC_ISER0              REG32(0xE000E100UL)
#define NVIC_ICER0              REG32(0xE000E180UL)
#define NVIC_ICPR0              REG32(0xE000E280UL)
#define NVIC_IPR_BYTE(n)        (*(volatile uint8_t *)(uintptr_t)(0xE000E400UL + (n)))

#define RTC0_IRQ_NUMBER         11U
#define RTC_COMPARE0_MASK       (1UL << 16U)
#define RTC_PRESCALER_8HZ       4095UL
#define RTC_TICKS_PER_SECOND    8UL
#define LFCLK_START_TIMEOUT     16000000UL

static volatile bool day_elapsed;
static uint32_t scheduled_ticks;

void RTC0_IRQHandler(void) {
    if (RTC0_EVENTS_COMPARE0 != 0U) {
        RTC0_EVENTS_COMPARE0 = 0U;
        /* nRF51 RTC has no COMPARE_CLEAR shortcut; clear it in the ISR. */
        RTC0_TASKS_CLEAR = 1U;
        day_elapsed = true;
    }
}

bool time_source_init(uint32_t first_interval_seconds) {
#if defined(LFCLK_SOURCE_xtal)
    CLOCK_LFCLKSRC = 1U;
    hal_log("LFCLK: external 32.768 kHz crystal\n");
#else
    CLOCK_LFCLKSRC = 0U;
    hal_log("LFCLK: internal RC oscillator\n");
#endif
    /* S130 deliberately leaves LFCLK running when disabled. */
    if ((CLOCK_LFCLKSTAT & (1UL << 16U)) == 0U) {
        CLOCK_EVENTS_LFCLKSTART = 0U;
        CLOCK_TASKS_LFCLKSTART = 1U;
        uint32_t timeout = LFCLK_START_TIMEOUT;
        while ((CLOCK_EVENTS_LFCLKSTART == 0U) && (timeout != 0U)) --timeout;
        if (timeout == 0U) {
            hal_log("LFCLK startup timeout\n");
            return false;
        }
    } else {
        hal_log("LFCLK already running after BLE shutdown\n");
    }

    RTC0_TASKS_STOP = 1U;
    RTC0_INTENCLR = 0xFFFFFFFFUL;
    RTC0_PRESCALER = RTC_PRESCALER_8HZ;
    RTC0_INTENSET = RTC_COMPARE0_MASK;
    NVIC_IPR_BYTE(RTC0_IRQ_NUMBER) = 0xC0U; /* Application priority 3. */
    NVIC_ICPR0 = (1UL << RTC0_IRQ_NUMBER);
    NVIC_ISER0 = (1UL << RTC0_IRQ_NUMBER);
    POWER_TASKS_LOWPWR = 1U;
    hal_log_u32("Daily wake interval seconds: ", WAKE_INTERVAL_SECONDS);
    return time_source_schedule(first_interval_seconds);
}

bool time_source_schedule(uint32_t interval_seconds) {
    uint32_t compare = interval_seconds * RTC_TICKS_PER_SECOND;
    if ((interval_seconds == 0U) ||
        (interval_seconds > (0x00FFFFFFUL / RTC_TICKS_PER_SECOND))) return false;
    RTC0_TASKS_STOP = 1U;
    RTC0_INTENCLR = 0xFFFFFFFFUL;
    RTC0_EVENTS_COMPARE0 = 0U;
    RTC0_TASKS_CLEAR = 1U;
    RTC0_PRESCALER = RTC_PRESCALER_8HZ;
    RTC0_CC0 = compare;
    RTC0_INTENSET = RTC_COMPARE0_MASK;
    scheduled_ticks = compare;
    day_elapsed = false;
    NVIC_IPR_BYTE(RTC0_IRQ_NUMBER) = 0xC0U;
    NVIC_ICPR0 = (1UL << RTC0_IRQ_NUMBER);
    NVIC_ISER0 = (1UL << RTC0_IRQ_NUMBER);
    RTC0_TASKS_START = 1U;
    return true;
}

time_source_event_t time_source_wait_event(void) {
    hal_log("Sleeping in System ON until RTC0 compare or button\n");
    for (;;) {
        if (day_elapsed) {
            day_elapsed = false;
            return TIME_SOURCE_DAY;
        }
        if (button_take_event()) return TIME_SOURCE_BUTTON;
        /* Exception entry wakes WFE; the loop also tolerates stale events. */
        __asm volatile("wfe");
    }
}

uint32_t time_source_pause(void) {
    RTC0_TASKS_STOP = 1U;
    NVIC_ICER0 = (1UL << RTC0_IRQ_NUMBER);
    uint32_t elapsed = RTC0_COUNTER & 0x00FFFFFFUL;
    if (day_elapsed || elapsed >= scheduled_ticks) return 1U;
    uint32_t remaining_ticks = scheduled_ticks - elapsed;
    return (remaining_ticks + RTC_TICKS_PER_SECOND - 1U) / RTC_TICKS_PER_SECOND;
}
