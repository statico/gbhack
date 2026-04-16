#ifndef FOV_H
#define FOV_H
#include "common.h"

/* Visibility bitmap -- 1 bit per cell, packed.
 * 40 * 30 = 1200 bits = 150 bytes. */
extern uint8_t fov_visible[150];

void fov_calculate(uint8_t px, uint8_t py) BANKED;
uint8_t fov_is_visible(uint8_t x, uint8_t y) BANKED;
void fov_clear(void) BANKED;

#endif
