#ifndef EPD_HAL_H
#define EPD_HAL_H

#include <stdbool.h>
#include <stdint.h>

void hal_init(void);
void hal_gpio_write(uint32_t pin, bool high);
bool hal_gpio_read(uint32_t pin);
bool hal_spi_write(uint8_t value);
void hal_delay_ms(uint32_t milliseconds);
void hal_log(const char *message);
void hal_log_u32(const char *prefix, uint32_t value);
void hal_epd_power_down(void);
void hal_idle(void);

/* Cortex-M0 NVIC priority registers accept word access only; byte writes are ignored. */
void hal_nvic_set_priority(uint32_t irq, uint8_t priority);

/* LFCLK is shared by the RTC0 day timer and the RTC1 sleeping delay timer. */
bool hal_lfclk_start(void);
void hal_lfclk_calibrate(void);

/* Millisecond timestamps for logging; valid for intervals shorter than 512 s. */
uint32_t hal_timestamp(void);
uint32_t hal_elapsed_ms(uint32_t since);

/*
 * One byte to the panel's own SPI flash on the shared SCLK/SDA lines. The
 * caller must first enable the UC8159 flash pass-through (command 0x65 = 1).
 */
bool hal_panel_flash_command(uint8_t command);

/* Puts the board's SPI NOR flash into deep power-down (no-op if BOARD_NOR_FLASH=0). */
void hal_board_flash_power_down(void);

#endif
