#ifndef INPUT_H
#define INPUT_H

#include <gb/gb.h>
#include <stdint.h>
#include "common.h"

extern uint8_t joy_current;
extern uint8_t joy_pressed;  // newly pressed this frame

void input_update(void);
uint8_t input_get_direction(void);  // returns DIR_* constant

#endif
