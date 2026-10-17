#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

void button_init(void);
bool button_is_pressed(void);
bool button_take_event(void);
void button_rearm_when_released(void);
bool button_wait_for_long_press(void);

/*
 * NFC field-detect wake (NFC_WAKE=1). The line only pulses while a reader's
 * field is present; it carries no data. All functions are no-ops otherwise.
 */
void nfc_wake_arm(void);
bool nfc_wake_take_event(void);
bool nfc_wake_confirm(void);
void nfc_wake_rearm_when_absent(void);

#endif
