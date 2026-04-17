#include "input.h"

uint8_t joy_current = 0;
uint8_t joy_pressed = 0;
uint8_t joy_released = 0;
static uint8_t joy_previous = 0;

/* Auto-repeat state for D-pad */
#define REPEAT_INITIAL_DELAY 12  /* frames before repeat starts (~200ms) */
#define REPEAT_RATE           5  /* frames between repeats (~83ms) */
static uint8_t repeat_timer = 0;
static uint8_t repeat_active = 0;

/* Tracks whether a direction was pressed at any point during the current B
   hold — if so, releasing B is not a "tap alone" and must not trigger rest. */
static uint8_t b_hold_used_direction = 0;

void input_update(void) {
    uint8_t dpad_mask;
    uint8_t dpad_pressed;

    joy_previous = joy_current;
    joy_current = joypad();
    joy_pressed = joy_current & ~joy_previous;
    joy_released = joy_previous & ~joy_current;

    /* Track D-pad hold for auto-repeat */
    dpad_mask = joy_current & (J_UP | J_DOWN | J_LEFT | J_RIGHT);
    if (dpad_mask) {
        if (repeat_timer < 255) repeat_timer++;
    } else {
        repeat_timer = 0;
        repeat_active = 0;
    }

    /* B-tap-alone tracking: a fresh B press starts a clean hold; any direction
       press during the hold disqualifies it; releasing B ends the hold. */
    if (joy_pressed & J_B) {
        b_hold_used_direction = 0;
    }
    dpad_pressed = joy_pressed & (J_UP | J_DOWN | J_LEFT | J_RIGHT);
    if ((joy_current & J_B) && dpad_pressed) {
        b_hold_used_direction = 1;
    }
}

uint8_t input_b_tapped_alone(void) {
    return (joy_released & J_B) && !b_hold_used_direction;
}

uint8_t input_get_direction(void) {
    uint8_t held = joy_current;
    uint8_t dpad_held;
    uint8_t do_repeat;

    /* Check if auto-repeat should fire */
    do_repeat = 0;
    dpad_held = held & (J_UP | J_DOWN | J_LEFT | J_RIGHT);
    if (dpad_held) {
        if (repeat_timer >= REPEAT_INITIAL_DELAY) {
            if (!repeat_active || repeat_timer >= REPEAT_INITIAL_DELAY + REPEAT_RATE) {
                do_repeat = 1;
                repeat_active = 1;
                repeat_timer = REPEAT_INITIAL_DELAY; /* reset to keep repeating */
            }
        }
    }

    /* B + D-pad = diagonal (on new press or repeat) */
    if (held & J_B) {
        if ((held & J_UP) && (held & J_RIGHT)) {
            if ((joy_pressed & (J_UP | J_RIGHT)) || do_repeat) return DIR_NE;
        }
        if ((held & J_UP) && (held & J_LEFT)) {
            if ((joy_pressed & (J_UP | J_LEFT)) || do_repeat) return DIR_NW;
        }
        if ((held & J_DOWN) && (held & J_RIGHT)) {
            if ((joy_pressed & (J_DOWN | J_RIGHT)) || do_repeat) return DIR_SE;
        }
        if ((held & J_DOWN) && (held & J_LEFT)) {
            if ((joy_pressed & (J_DOWN | J_LEFT)) || do_repeat) return DIR_SW;
        }
    }

    /* Cardinal directions (on new press or repeat) */
    if ((joy_pressed & J_UP)    || (do_repeat && (dpad_held & J_UP)))    return DIR_N;
    if ((joy_pressed & J_DOWN)  || (do_repeat && (dpad_held & J_DOWN)))  return DIR_S;
    if ((joy_pressed & J_LEFT)  || (do_repeat && (dpad_held & J_LEFT)))  return DIR_W;
    if ((joy_pressed & J_RIGHT) || (do_repeat && (dpad_held & J_RIGHT))) return DIR_E;

    return DIR_NONE;
}
