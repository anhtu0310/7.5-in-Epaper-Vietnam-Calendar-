#include "button.h"

#include <stdint.h>

#include "hal.h"

#ifndef BLE_BUTTON_PIN
#define BLE_BUTTON_PIN 14U
#endif
#ifndef BLE_BUTTON_HOLD_MS
#define BLE_BUTTON_HOLD_MS 3000U
#endif

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define GPIO_IN         REG32(0x50000510UL)
#define GPIO_PIN_CNF(n) REG32(0x50000700UL + 4UL * (uint32_t)(n))
#define GPIOTE_EVENTS_PORT REG32(0x4000617CUL)
#define GPIOTE_INTENSET    REG32(0x40006304UL)
#define GPIOTE_INTENCLR    REG32(0x40006308UL)
#define NVIC_ISER0         REG32(0xE000E100UL)
#define NVIC_ICPR0         REG32(0xE000E280UL)
#define NVIC_IPR_BYTE(n)   (*(volatile uint8_t *)(uintptr_t)(0xE000E400UL + (n)))

#define GPIOTE_IRQ_NUMBER 6U
#define GPIOTE_PORT_MASK  (1UL << 31U)
#define GPIO_INPUT_PULLUP (3UL << 2U)
#define GPIO_SENSE_LOW    (3UL << 16U)

static volatile bool pressed_event;

void GPIOTE_IRQHandler(void) {
    if (GPIOTE_EVENTS_PORT != 0U) {
        GPIOTE_EVENTS_PORT = 0U;
        /* Disable level sensing while held to avoid an interrupt storm. */
        GPIO_PIN_CNF(BLE_BUTTON_PIN) = GPIO_INPUT_PULLUP;
        pressed_event = true;
    }
}

void button_init(void) {
    pressed_event = false;
    GPIO_PIN_CNF(BLE_BUTTON_PIN) = GPIO_INPUT_PULLUP | GPIO_SENSE_LOW;
    GPIOTE_EVENTS_PORT = 0U;
    GPIOTE_INTENCLR = 0xFFFFFFFFUL;
    GPIOTE_INTENSET = GPIOTE_PORT_MASK;
    NVIC_IPR_BYTE(GPIOTE_IRQ_NUMBER) = 0xC0U; /* Application priority 3. */
    NVIC_ICPR0 = (1UL << GPIOTE_IRQ_NUMBER);
    NVIC_ISER0 = (1UL << GPIOTE_IRQ_NUMBER);
}

bool button_is_pressed(void) {
    return (GPIO_IN & (1UL << BLE_BUTTON_PIN)) == 0U;
}

bool button_take_event(void) {
    bool result = pressed_event;
    pressed_event = false;
    return result;
}

void button_rearm_when_released(void) {
    /* Release does not generate an interrupt while SENSE is disabled. */
    while (button_is_pressed()) hal_delay_ms(10U);
    GPIOTE_EVENTS_PORT = 0U;
    GPIO_PIN_CNF(BLE_BUTTON_PIN) = GPIO_INPUT_PULLUP | GPIO_SENSE_LOW;
}

bool button_wait_for_long_press(void) {
    if (!button_is_pressed()) return false;
    for (uint32_t elapsed = 0U; elapsed < BLE_BUTTON_HOLD_MS; elapsed += 10U) {
        hal_delay_ms(10U);
        if (!button_is_pressed()) return false;
    }
    return true;
}
