#include "hal.h"

#include "rtt.h"

#ifndef EPD_MOSI_PIN
#error "EPD_MOSI_PIN must be supplied by the Makefile"
#endif
#ifndef EPD_SCLK_PIN
#error "EPD_SCLK_PIN must be supplied by the Makefile"
#endif
#ifndef EPD_CS_PIN
#error "EPD_CS_PIN must be supplied by the Makefile"
#endif
#ifndef EPD_DC_PIN
#error "EPD_DC_PIN must be supplied by the Makefile"
#endif
#ifndef EPD_RST_PIN
#error "EPD_RST_PIN must be supplied by the Makefile"
#endif
#ifndef EPD_BUSY_PIN
#error "EPD_BUSY_PIN must be supplied by the Makefile"
#endif
#ifndef EPD_BS_PIN
#error "EPD_BS_PIN must be supplied by the Makefile"
#endif
#ifndef EPD_USE_HARDWARE_SPI
#define EPD_USE_HARDWARE_SPI 1
#endif
#ifndef EPD_SPI_FREQUENCY_KHZ
#define EPD_SPI_FREQUENCY_KHZ 2000
#endif
#ifndef EPD_IDLE_PINS_HIGH
#define EPD_IDLE_PINS_HIGH 1
#endif
#ifndef EPD_PANEL_FLASH
#define EPD_PANEL_FLASH 0
#endif
#ifndef BOARD_NOR_FLASH
#define BOARD_NOR_FLASH 0
#endif
#ifndef LFCLK_RC_CALIBRATE
#define LFCLK_RC_CALIBRATE 1
#endif
#if !defined(LFCLK_SOURCE_rc) && !defined(LFCLK_SOURCE_xtal)
#error "LFCLK_SOURCE must be rc or xtal"
#endif
#if EPD_PANEL_FLASH && !defined(EPD_PANEL_FLASH_CS_PIN)
#error "EPD_PANEL_FLASH=1 requires EPD_PANEL_FLASH_CS_PIN"
#endif
#if BOARD_NOR_FLASH && (!defined(NOR_CS_PIN) || !defined(NOR_SCK_PIN) || !defined(NOR_MOSI_PIN))
#error "BOARD_NOR_FLASH=1 requires NOR_CS_PIN, NOR_SCK_PIN and NOR_MOSI_PIN"
#endif

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define GPIO_OUTSET     REG32(0x50000508UL)
#define GPIO_OUTCLR     REG32(0x5000050CUL)
#define GPIO_IN         REG32(0x50000510UL)
#define GPIO_DIRSET     REG32(0x50000518UL)
#define GPIO_PIN_CNF(n) REG32(0x50000700UL + ((uint32_t)(n) * 4UL))

#define CLOCK_TASKS_HFCLKSTART    REG32(0x40000000UL)
#define CLOCK_TASKS_HFCLKSTOP     REG32(0x40000004UL)
#define CLOCK_TASKS_LFCLKSTART    REG32(0x40000008UL)
#define CLOCK_TASKS_CAL           REG32(0x40000010UL)
#define CLOCK_EVENTS_HFCLKSTARTED REG32(0x40000100UL)
#define CLOCK_EVENTS_LFCLKSTARTED REG32(0x40000104UL)
#define CLOCK_EVENTS_DONE         REG32(0x4000010CUL)
#define CLOCK_LFCLKSTAT           REG32(0x40000418UL)
#define CLOCK_LFCLKSRC            REG32(0x40000518UL)
#define POWER_TASKS_LOWPWR        REG32(0x4000007CUL)

#define RTC1_TASKS_START     REG32(0x40011000UL)
#define RTC1_TASKS_STOP      REG32(0x40011004UL)
#define RTC1_EVENTS_COMPARE0 REG32(0x40011140UL)
#define RTC1_INTENSET        REG32(0x40011304UL)
#define RTC1_INTENCLR        REG32(0x40011308UL)
#define RTC1_COUNTER         REG32(0x40011504UL)
#define RTC1_PRESCALER       REG32(0x40011508UL)
#define RTC1_CC0             REG32(0x40011540UL)
#define NVIC_ICPR0           REG32(0xE000E280UL)
#define SCB_SCR              REG32(0xE000ED10UL)

#if EPD_USE_HARDWARE_SPI
#define SPI0_EVENTS_READY REG32(0x40003108UL)
#define SPI0_ENABLE       REG32(0x40003500UL)
#define SPI0_PSELSCK      REG32(0x40003508UL)
#define SPI0_PSELMOSI     REG32(0x4000350CUL)
#define SPI0_PSELMISO     REG32(0x40003510UL)
#define SPI0_RXD          REG32(0x40003518UL)
#define SPI0_TXD          REG32(0x4000351CUL)
#define SPI0_FREQUENCY    REG32(0x40003524UL)
#define SPI0_CONFIG       REG32(0x40003554UL)
#define SPI0_POWER        REG32(0x40003FFCUL)
#endif

#define GPIO_CNF_OUTPUT_H0H1 ((1UL << 0) | (1UL << 1) | (3UL << 8))
#define GPIO_CNF_INPUT_NOPULL 0UL
#define GPIO_CNF_INPUT_DISCONNECT (1UL << 1U)
#define LFCLK_RUNNING         (1UL << 16U)
#define CLOCK_START_TIMEOUT   16000000UL
#define RTC1_IRQ_MASK         (1UL << 17U)
#define RTC_COMPARE0_MASK     (1UL << 16U)
#define RTC_COUNTER_MASK      0x00FFFFFFUL
#define SCR_SEVONPEND         (1UL << 4U)
#define LFCLK_HZ              32768UL

#if EPD_USE_HARDWARE_SPI
#if EPD_SPI_FREQUENCY_KHZ == 125
#define SPI_FREQUENCY_SETTING 0x02000000UL
#elif EPD_SPI_FREQUENCY_KHZ == 250
#define SPI_FREQUENCY_SETTING 0x04000000UL
#elif EPD_SPI_FREQUENCY_KHZ == 500
#define SPI_FREQUENCY_SETTING 0x08000000UL
#elif EPD_SPI_FREQUENCY_KHZ == 1000
#define SPI_FREQUENCY_SETTING 0x10000000UL
#elif EPD_SPI_FREQUENCY_KHZ == 2000
#define SPI_FREQUENCY_SETTING 0x20000000UL
#elif EPD_SPI_FREQUENCY_KHZ == 4000
#define SPI_FREQUENCY_SETTING 0x40000000UL
#elif EPD_SPI_FREQUENCY_KHZ == 8000
#define SPI_FREQUENCY_SETTING 0x80000000UL
#else
#error "EPD_SPI_FREQUENCY_KHZ must be 125, 250, 500, 1000, 2000, 4000 or 8000"
#endif
#define SPI_PIN_DISCONNECTED    0xFFFFFFFFUL
#define SPI_READY_TIMEOUT_LOOPS 1000000UL
#endif

static bool sleep_timer_running;

static void delay_approximately_1ms(void) {
    /* Four cycles per loop on Cortex-M0: SUBS + NOP + taken BNE. */
    uint32_t loops = 4000U;
    __asm volatile(
        ".syntax unified\n"
        "1:\n"
        "subs %0, #1\n"
        "nop\n"
        "bne.n 1b\n"
        ".syntax divided\n"
        : "+l"(loops));
}

static void delay_approximately_50us(void) {
    uint32_t loops = 200U;
    __asm volatile(
        ".syntax unified\n"
        "1:\n"
        "subs %0, #1\n"
        "nop\n"
        "bne.n 1b\n"
        ".syntax divided\n"
        : "+l"(loops));
}

static void gpio_output(uint32_t pin, bool initial_high) {
    hal_gpio_write(pin, initial_high);
    GPIO_PIN_CNF(pin) = GPIO_CNF_OUTPUT_H0H1;
    GPIO_DIRSET = (1UL << pin);
}

static bool lfclk_running(void) {
    return (CLOCK_LFCLKSTAT & LFCLK_RUNNING) != 0U;
}

/*
 * RTC1 runs at 32768 Hz while the panel is being driven, so delays and BUSY
 * polling sleep in System ON instead of spinning the CPU. The BLE window
 * reprograms RTC1 only while no panel session is active.
 */
static void sleep_timer_start(void) {
    RTC1_TASKS_STOP = 1U;
    RTC1_INTENCLR = 0xFFFFFFFFUL;
    RTC1_EVENTS_COMPARE0 = 0U;
    delay_approximately_50us(); /* PRESCALER is writable only once RTC1 has stopped. */
    RTC1_PRESCALER = 0U;
    NVIC_ICPR0 = RTC1_IRQ_MASK;
    RTC1_TASKS_START = 1U;
    sleep_timer_running = true;
}

static void sleep_timer_stop(void) {
    RTC1_TASKS_STOP = 1U;
    RTC1_INTENCLR = 0xFFFFFFFFUL;
    RTC1_EVENTS_COMPARE0 = 0U;
    NVIC_ICPR0 = RTC1_IRQ_MASK;
    sleep_timer_running = false;
}

static void sleep_ticks(uint32_t ticks) {
    bool started_here = !sleep_timer_running;
    if (started_here) sleep_timer_start();
    /* nRF51 RTC may miss a compare value only 0 or 1 tick ahead of COUNTER. */
    if (ticks < 2U) ticks = 2U;

    RTC1_EVENTS_COMPARE0 = 0U;
    NVIC_ICPR0 = RTC1_IRQ_MASK;
    RTC1_CC0 = (RTC1_COUNTER + ticks) & RTC_COUNTER_MASK;
    RTC1_INTENSET = RTC_COMPARE0_MASK;

    /* RTC1's IRQ stays disabled in the NVIC; SEVONPEND turns its pending edge into a WFE wake. */
    uint32_t saved_scr = SCB_SCR;
    SCB_SCR = saved_scr | SCR_SEVONPEND;
    while (RTC1_EVENTS_COMPARE0 == 0U) __asm volatile("wfe");
    SCB_SCR = saved_scr;

    RTC1_EVENTS_COMPARE0 = 0U;
    RTC1_INTENCLR = RTC_COMPARE0_MASK;
    NVIC_ICPR0 = RTC1_IRQ_MASK;
    if (started_here) sleep_timer_stop();
}

bool hal_lfclk_start(void) {
    if (lfclk_running()) return true;
#if defined(LFCLK_SOURCE_xtal)
    CLOCK_LFCLKSRC = 1U;
    rtt_write("LFCLK: external 32.768 kHz crystal\n");
#else
    CLOCK_LFCLKSRC = 0U;
    rtt_write("LFCLK: internal RC oscillator\n");
#endif
    CLOCK_EVENTS_LFCLKSTARTED = 0U;
    CLOCK_TASKS_LFCLKSTART = 1U;
    uint32_t timeout = CLOCK_START_TIMEOUT;
    while ((CLOCK_EVENTS_LFCLKSTARTED == 0U) && (timeout != 0U)) --timeout;
    if (timeout == 0U) {
        rtt_write("LFCLK startup timeout\n");
        return false;
    }
    hal_lfclk_calibrate();
    return true;
}

void hal_lfclk_calibrate(void) {
#if defined(LFCLK_SOURCE_rc) && LFCLK_RC_CALIBRATE
    /* The RC oscillator is ±2% uncalibrated; calibration against HFXO needs a few ms. */
    if (!lfclk_running()) return;
    CLOCK_EVENTS_HFCLKSTARTED = 0U;
    CLOCK_TASKS_HFCLKSTART = 1U;
    uint32_t timeout = CLOCK_START_TIMEOUT;
    while ((CLOCK_EVENTS_HFCLKSTARTED == 0U) && (timeout != 0U)) --timeout;
    if (timeout != 0U) {
        CLOCK_EVENTS_DONE = 0U;
        CLOCK_TASKS_CAL = 1U;
        timeout = CLOCK_START_TIMEOUT;
        while ((CLOCK_EVENTS_DONE == 0U) && (timeout != 0U)) --timeout;
        CLOCK_EVENTS_DONE = 0U;
    }
    CLOCK_TASKS_HFCLKSTOP = 1U;
    rtt_write(timeout != 0U ? "LFCLK RC calibrated\n" : "LFCLK RC calibration timeout\n");
#endif
}

uint32_t hal_timestamp(void) {
    return sleep_timer_running ? (RTC1_COUNTER & RTC_COUNTER_MASK) : 0U;
}

uint32_t hal_elapsed_ms(uint32_t since) {
    if (!sleep_timer_running) return 0U;
    uint32_t ticks = ((RTC1_COUNTER & RTC_COUNTER_MASK) - since) & RTC_COUNTER_MASK;
    return (ticks * 125UL) / 4096UL; /* ticks * 1000 / 32768 without overflow. */
}

void hal_init(void) {
    rtt_init();
    (void)hal_lfclk_start();
    if (lfclk_running()) sleep_timer_start();

    gpio_output(EPD_CS_PIN, true);
    gpio_output(EPD_DC_PIN, false);
    gpio_output(EPD_RST_PIN, true);
    gpio_output(EPD_BS_PIN, false); /* 4-wire SPI selection used by this board. */
    gpio_output(EPD_SCLK_PIN, false);
    gpio_output(EPD_MOSI_PIN, false);
#if EPD_PANEL_FLASH
    gpio_output(EPD_PANEL_FLASH_CS_PIN, true);
#endif

    GPIO_PIN_CNF(EPD_BUSY_PIN) = GPIO_CNF_INPUT_NOPULL;

#if EPD_USE_HARDWARE_SPI
    /* Reset the shared SPI0/TWI0 peripheral before selecting SPI mode. */
    SPI0_ENABLE = 0U;
    SPI0_POWER = 0U;
    delay_approximately_1ms();
    SPI0_POWER = 1U;
    delay_approximately_1ms();
    SPI0_PSELSCK = EPD_SCLK_PIN;
    SPI0_PSELMOSI = EPD_MOSI_PIN;
    SPI0_PSELMISO = SPI_PIN_DISCONNECTED;
    SPI0_FREQUENCY = SPI_FREQUENCY_SETTING;
    SPI0_CONFIG = 0U; /* Mode 0, MSB first. */
    SPI0_ENABLE = 1U;
#endif
}

void hal_epd_power_down(void) {
#if EPD_USE_HARDWARE_SPI
    SPI0_ENABLE = 0U;
    SPI0_EVENTS_READY = 0U;
    SPI0_PSELSCK = SPI_PIN_DISCONNECTED;
    SPI0_PSELMOSI = SPI_PIN_DISCONNECTED;
    SPI0_POWER = 0U;
#endif
    /*
     * EPD_IDLE_PINS_HIGH=1 matches the factory firmware, which leaves every
     * panel control line high (except BS) while the controller deep-sleeps.
     */
    hal_gpio_write(EPD_CS_PIN, EPD_IDLE_PINS_HIGH);
    hal_gpio_write(EPD_DC_PIN, EPD_IDLE_PINS_HIGH);
    hal_gpio_write(EPD_RST_PIN, EPD_IDLE_PINS_HIGH);
    hal_gpio_write(EPD_SCLK_PIN, EPD_IDLE_PINS_HIGH);
    hal_gpio_write(EPD_MOSI_PIN, EPD_IDLE_PINS_HIGH);
#if EPD_PANEL_FLASH
    hal_gpio_write(EPD_PANEL_FLASH_CS_PIN, true);
#endif
    /* BUSY is otherwise a floating enabled input when the panel is absent/off. */
    GPIO_PIN_CNF(EPD_BUSY_PIN) = GPIO_CNF_INPUT_DISCONNECT;
    sleep_timer_stop();
    CLOCK_TASKS_HFCLKSTOP = 1U;
    POWER_TASKS_LOWPWR = 1U;
}

void hal_gpio_write(uint32_t pin, bool high) {
    if (high) {
        GPIO_OUTSET = (1UL << pin);
    } else {
        GPIO_OUTCLR = (1UL << pin);
    }
}

bool hal_gpio_read(uint32_t pin) {
    return (GPIO_IN & (1UL << pin)) != 0U;
}

/* One byte on SCLK/MOSI without touching any chip select. */
static bool spi_transfer(uint8_t value) {
#if EPD_USE_HARDWARE_SPI
    uint32_t timeout = SPI_READY_TIMEOUT_LOOPS;

    SPI0_EVENTS_READY = 0U;
    SPI0_TXD = value;
    while ((SPI0_EVENTS_READY == 0U) && (timeout != 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        rtt_write("SPI0 READY timeout\n");
        return false;
    }
    (void)SPI0_RXD;
    return true;
#else
    /* Conservative software SPI for the first hardware bring-up. Mode 0, MSB first. */
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
        hal_gpio_write(EPD_SCLK_PIN, false);
        hal_gpio_write(EPD_MOSI_PIN, (value & mask) != 0U);
        __asm volatile("nop\n nop");
        hal_gpio_write(EPD_SCLK_PIN, true);
        __asm volatile("nop\n nop");
    }
    hal_gpio_write(EPD_SCLK_PIN, false);
    return true;
#endif
}

bool hal_spi_write(uint8_t value) {
    /* Waveshare's SpiTransfer() toggles CS around every individual byte. */
    hal_gpio_write(EPD_CS_PIN, false);
    bool ok = spi_transfer(value);
    hal_gpio_write(EPD_CS_PIN, true);
    return ok;
}

bool hal_panel_flash_command(uint8_t command) {
#if EPD_PANEL_FLASH
    hal_gpio_write(EPD_PANEL_FLASH_CS_PIN, false);
    bool ok = spi_transfer(command);
    hal_gpio_write(EPD_PANEL_FLASH_CS_PIN, true);
    return ok;
#else
    (void)command;
    return true;
#endif
}

void hal_board_flash_power_down(void) {
#if BOARD_NOR_FLASH
    static const uint8_t deep_power_down = 0xB9U;

    gpio_output(NOR_CS_PIN, true);
    gpio_output(NOR_SCK_PIN, false);
    gpio_output(NOR_MOSI_PIN, false);
    hal_delay_ms(10U); /* Let the flash finish its power-up before the first command. */

    /* Mode 0 bit-bang; the flash ignores 0xB9 if it is already in deep power-down. */
    hal_gpio_write(NOR_CS_PIN, false);
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
        hal_gpio_write(NOR_MOSI_PIN, (deep_power_down & mask) != 0U);
        hal_gpio_write(NOR_SCK_PIN, true);
        hal_gpio_write(NOR_SCK_PIN, false);
    }
    hal_gpio_write(NOR_CS_PIN, true);

    /* Idle levels used by the factory firmware: CS, SCK and MOSI all driven high. */
    hal_gpio_write(NOR_SCK_PIN, true);
    hal_gpio_write(NOR_MOSI_PIN, true);
    rtt_write("Board NOR flash: deep power-down\n");
#endif
}

void hal_delay_ms(uint32_t milliseconds) {
    if (!lfclk_running()) {
        /* Before LFCLK starts there is no sleep timer; spin instead. */
        while (milliseconds-- != 0U) delay_approximately_1ms();
        return;
    }
    while (milliseconds != 0U) {
        uint32_t chunk = (milliseconds > 1000U) ? 1000U : milliseconds;
        sleep_ticks((chunk * LFCLK_HZ + 999U) / 1000U);
        milliseconds -= chunk;
    }
}

void hal_log(const char *message) {
    rtt_write(message);
}

void hal_log_u32(const char *prefix, uint32_t value) {
    rtt_write(prefix);
    rtt_write_u32(value);
    rtt_write("\n");
}

void hal_idle(void) {
    __asm volatile("wfe");
}

void hal_nvic_set_priority(uint32_t irq, uint8_t priority) {
    volatile uint32_t *ipr = (volatile uint32_t *)(uintptr_t)(0xE000E400UL + (irq & ~3UL));
    uint32_t shift = (irq & 3UL) * 8UL;
    *ipr = (*ipr & ~(0xFFUL << shift)) | ((uint32_t)priority << shift);
}
