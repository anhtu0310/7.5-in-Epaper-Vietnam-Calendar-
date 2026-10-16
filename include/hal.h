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

#endif
