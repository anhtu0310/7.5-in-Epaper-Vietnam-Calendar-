#include "button.h"

#include <stdint.h>

#include "hal.h"

#ifndef BLE_BUTTON_PIN
#define BLE_BUTTON_PIN 14U
#endif
#ifndef BLE_BUTTON_HOLD_MS
#define BLE_BUTTON_HOLD_MS 3000U
#endif
#ifndef NFC_WAKE
#define NFC_WAKE 0
#endif
#if NFC_WAKE && !defined(NFC_WAKE_PIN)
#error "NFC_WAKE=1 requires NFC_WAKE_PIN"
#endif
#ifndef NFC_CONFIRM_MS
#define NFC_CONFIRM_MS 2000U
#endif
#ifndef NFC_ABSENT_MS
#define NFC_ABSENT_MS 1500U
#endif
#ifndef NFC_REARM_TIMEOUT_MS
#define NFC_REARM_TIMEOUT_MS 20000U
#endif

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define GPIO_IN         REG32(0x50000510UL)
#define GPIO_PIN_CNF(n) REG32(0x50000700UL + 4UL * (uint32_t)(n))
#define GPIOTE_EVENTS_PORT REG32(0x4000617CUL)
#define GPIOTE_INTENSET    REG32(0x40006304UL)
#define GPIOTE_INTENCLR    REG32(0x40006308UL)
#define NVIC_ISER0         REG32(0xE000E100UL)
#define NVIC_ICPR0         REG32(0xE000E280UL)

#define GPIOTE_IRQ_NUMBER 6U
#define GPIOTE_PORT_MASK  (1UL << 31U)
#define GPIO_INPUT_NOPULL 0UL
#define GPIO_INPUT_PULLUP (3UL << 2U)
#define GPIO_SENSE_MASK   (3UL << 16U)
#define GPIO_SENSE_HIGH   (2UL << 16U)
#define GPIO_SENSE_LOW    (3UL << 16U)

static volatile bool pressed_event;
#if NFC_WAKE
static volatile bool nfc_event;
#endif

void GPIOTE_IRQHandler(void) {
    if (GPIOTE_EVENTS_PORT == 0U) return;
    GPIOTE_EVENTS_PORT = 0U;

    bool button_armed = (GPIO_PIN_CNF(BLE_BUTTON_PIN) & GPIO_SENSE_MASK) == GPIO_SENSE_LOW;
    if (button_armed && (GPIO_IN & (1UL << BLE_BUTTON_PIN)) == 0U) {
        /* Disable level sensing while held to avoid an interrupt storm. */
        GPIO_PIN_CNF(BLE_BUTTON_PIN) = GPIO_INPUT_PULLUP;
        pressed_event = true;
        return;
    }
#if NFC_WAKE
    if ((GPIO_PIN_CNF(NFC_WAKE_PIN) & GPIO_SENSE_MASK) == GPIO_SENSE_HIGH) {
        /*
         * The field-detect pulse lasts only a few ms and may already be over;
         * with the button released, NFC is the only other armed source.
         */
        GPIO_PIN_CNF(NFC_WAKE_PIN) = GPIO_INPUT_NOPULL;
        nfc_event = true;
    }
#endif
}

void button_init(void) {
    pressed_event = false;
    GPIO_PIN_CNF(BLE_BUTTON_PIN) = GPIO_INPUT_PULLUP | GPIO_SENSE_LOW;
    GPIOTE_EVENTS_PORT = 0U;
    GPIOTE_INTENCLR = 0xFFFFFFFFUL;
    GPIOTE_INTENSET = GPIOTE_PORT_MASK;
    hal_nvic_set_priority(GPIOTE_IRQ_NUMBER, 0xC0U); /* Application priority 3; S130 rejects 0. */
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

#if NFC_WAKE

void nfc_wake_arm(void) {
    /* The detector drives the line itself, like the factory firmware's no-pull input. */
    GPIO_PIN_CNF(NFC_WAKE_PIN) = GPIO_INPUT_NOPULL | GPIO_SENSE_HIGH;
}

bool nfc_wake_take_event(void) {
    bool result = nfc_event;
    nfc_event = false;
    return result;
}

bool nfc_wake_confirm(void) {
    /* A phone polls about every 0.5 s, so a real tap produces a second pulse; noise does not. */
    for (uint32_t waited = 0U; (GPIO_IN & (1UL << NFC_WAKE_PIN)) != 0U && waited < 50U; ++waited)
        hal_delay_ms(1U);
    nfc_event = false;
    nfc_wake_arm();
    for (uint32_t elapsed = 0U; elapsed < NFC_CONFIRM_MS; elapsed += 10U) {
        if (nfc_event) break;
        hal_delay_ms(10U);
    }
    return nfc_wake_take_event();
}

void nfc_wake_rearm_when_absent(void) {
    /* Re-arm only once the field has been gone for a while so a resting phone cannot retrigger. */
    for (uint32_t elapsed = 0U; elapsed < NFC_REARM_TIMEOUT_MS;) {
        nfc_event = false;
        nfc_wake_arm();
        uint32_t quiet = 0U;
        while (!nfc_event && quiet < NFC_ABSENT_MS) {
            hal_delay_ms(50U);
            quiet += 50U;
        }
        if (!nfc_event) return;
        elapsed += quiet;
    }
    GPIO_PIN_CNF(NFC_WAKE_PIN) = GPIO_INPUT_NOPULL;
    nfc_event = false;
    hal_log("NFC field still present; NFC wake paused until the next daily refresh\n");
}

#else

void nfc_wake_arm(void) { }
bool nfc_wake_take_event(void) { return false; }
bool nfc_wake_confirm(void) { return false; }
void nfc_wake_rearm_when_absent(void) { }

#endif
