#ifndef RENDER_H
#define RENDER_H
#include "common.h"

/* Camera position (top-left corner of viewport in map coords) */
extern uint8_t camera_x, camera_y;

void render_init(void) BANKED;           /* Load palettes, clear screen */
void render_update(void) BANKED;         /* Redraw visible viewport */
void render_status_bar(void) BANKED;     /* Update window layer status */
void render_message(const char *msg) BANKED;  /* Show message on bottom rows */
void render_clear_message(void) BANKED;
void render_full_redraw(void) BANKED;    /* Force full viewport redraw */
void render_tile_at(uint8_t map_x, uint8_t map_y) BANKED;  /* Update single tile */

#endif
