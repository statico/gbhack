#include "input.h"

uint8_t joy_current = 0;
uint8_t joy_pressed = 0;
static uint8_t joy_previous = 0;

void input_update(void) {
    joy_previous = joy_current;
    joy_current = joypad();
    joy_pressed = joy_current & ~joy_previous;
}

uint8_t input_get_direction(void) {
    uint8_t held = joy_current;

    // B + D-pad = diagonal
    if (held & J_B) {
        if ((held & J_UP) && (held & J_RIGHT)) return DIR_NE;
        if ((held & J_UP) && (held & J_LEFT))  return DIR_NW;
        if ((held & J_DOWN) && (held & J_RIGHT)) return DIR_SE;
        if ((held & J_DOWN) && (held & J_LEFT))  return DIR_SW;
    }

    // Cardinal directions (only on new press)
    if (joy_pressed & J_UP)    return DIR_N;
    if (joy_pressed & J_DOWN)  return DIR_S;
    if (joy_pressed & J_LEFT)  return DIR_W;
    if (joy_pressed & J_RIGHT) return DIR_E;

    return DIR_NONE;
}
