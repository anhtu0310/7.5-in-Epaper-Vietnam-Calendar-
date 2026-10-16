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

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define GPIO_OUTSET     REG32(0x50000508UL)
#define GPIO_OUTCLR     REG32(0x5000050CUL)
#define GPIO_IN         REG32(0x50000510UL)
#define GPIO_DIRSET     REG32(0x50000518UL)
#define GPIO_PIN_CNF(n) REG32(0x50000700UL + ((uint32_t)(n) * 4UL))
#define CLOCK_TASKS_HFCLKSTOP REG32(0x40000004UL)
#define POWER_TASKS_LOWPWR    REG32(0x4000007CUL)

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
#if EPD_USE_HARDWARE_SPI
#define SPI_FREQUENCY_2M        0x20000000UL
#define SPI_PIN_DISCONNECTED    0xFFFFFFFFUL
#define SPI_READY_TIMEOUT_LOOPS 1000000UL
#endif

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

static void gpio_output(uint32_t pin, bool initial_high) {
    hal_gpio_write(pin, initial_high);
    GPIO_PIN_CNF(pin) = GPIO_CNF_OUTPUT_H0H1;
    GPIO_DIRSET = (1UL << pin);
}

void hal_init(void) {
    rtt_init();

    gpio_output(EPD_CS_PIN, true);
    gpio_output(EPD_DC_PIN, false);
    gpio_output(EPD_RST_PIN, true);
    gpio_output(EPD_BS_PIN, false); /* 4-wire SPI selection used by this board. */
    gpio_output(EPD_SCLK_PIN, false);
    gpio_output(EPD_MOSI_PIN, false);

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
    SPI0_FREQUENCY = SPI_FREQUENCY_2M;
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
    hal_gpio_write(EPD_CS_PIN, false);
    hal_gpio_write(EPD_DC_PIN, false);
    hal_gpio_write(EPD_RST_PIN, false);
    hal_gpio_write(EPD_SCLK_PIN, false);
    hal_gpio_write(EPD_MOSI_PIN, false);
    /* BUSY is otherwise a floating enabled input when the panel is absent/off. */
    GPIO_PIN_CNF(EPD_BUSY_PIN) = GPIO_CNF_INPUT_DISCONNECT;
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

bool hal_spi_write(uint8_t value) {
#if EPD_USE_HARDWARE_SPI
    uint32_t timeout = SPI_READY_TIMEOUT_LOOPS;

    SPI0_EVENTS_READY = 0U;

    /* Waveshare's SpiTransfer() toggles CS around every individual byte. */
    hal_gpio_write(EPD_CS_PIN, false);
    SPI0_TXD = value;
    while ((SPI0_EVENTS_READY == 0U) && (timeout != 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        hal_gpio_write(EPD_CS_PIN, true);
        rtt_write("SPI0 READY timeout\n");
        return false;
    }
    (void)SPI0_RXD;
    hal_gpio_write(EPD_CS_PIN, true);
    return true;
#else
    /*
     * Conservative software SPI for the first hardware bring-up. Mode 0,
     * MSB first, with the same per-byte CS pulse used by Waveshare.
     */
    hal_gpio_write(EPD_CS_PIN, false);
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
        hal_gpio_write(EPD_SCLK_PIN, false);
        hal_gpio_write(EPD_MOSI_PIN, (value & mask) != 0U);
        __asm volatile("nop\n nop");
        hal_gpio_write(EPD_SCLK_PIN, true);
        __asm volatile("nop\n nop");
    }
    hal_gpio_write(EPD_SCLK_PIN, false);
    hal_gpio_write(EPD_CS_PIN, true);
    return true;
#endif
}

void hal_delay_ms(uint32_t milliseconds) {
    /* Avoid relying on SysTick while debugging early bare-metal startup. */
    while (milliseconds-- != 0U) delay_approximately_1ms();
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
