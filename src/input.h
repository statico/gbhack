#ifndef INPUT_H
#define INPUT_H

#include <gb/gb.h>
#include <stdint.h>
#include "common.h"

extern uint8_t joy_current;
extern uint8_t joy_pressed;  // newly pressed this frame
extern uint8_t joy_released; // newly released this frame

void input_update(void);
uint8_t input_get_direction(void);  // returns DIR_* constant

/* 1 if B was just released AND no direction was pressed during the hold
   (i.e. user tapped B alone, intending rest/pickup — not diagonal movement) */
uint8_t input_b_tapped_alone(void);

#endif
