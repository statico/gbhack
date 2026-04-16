#ifndef RENDER_H
#define RENDER_H
#include "common.h"

/* Camera position (top-left corner of viewport in map coords) */
extern uint8_t camera_x, camera_y;

void render_init(void);           /* Load palettes, clear screen */
void render_update(void);         /* Redraw visible viewport */
void render_status_bar(void);     /* Update window layer status */
void render_message(const char *msg);  /* Show message on bottom rows */
void render_clear_message(void);
void render_full_redraw(void);    /* Force full viewport redraw */
void render_tile_at(uint8_t map_x, uint8_t map_y);  /* Update single tile */

#endif
