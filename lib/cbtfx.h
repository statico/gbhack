/*

CoffeeBaT-FX (CBT-FX) — public API header.

Very simple sound effect driver by Coffee 'Valen' Bat. See cbtfx.c for the
full origin notice. Upstream: https://github.com/datmobiledev/cbtfx (linked
from this project's README; currently appears to be offline).

*/

#ifndef CBTFX_H_INCLUDE
#define CBTFX_H_INCLUDE

#include <gb/gb.h>

void CBTFX_update(void);
void CBTFX_init(const unsigned char * SFX);

// 0 = Panning won't be reset after an SFX, 1 = Panning will be set to 0XFF after an SFX plays.
#define MONO_MUSIC 0

#endif
