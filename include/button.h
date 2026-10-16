#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

void button_init(void);
bool button_is_pressed(void);
bool button_take_event(void);
void button_rearm_when_released(void);
bool button_wait_for_long_press(void);

#endif
