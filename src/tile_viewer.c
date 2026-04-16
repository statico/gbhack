/*
 * tile_viewer.c — Standalone tile viewer ROM.
 * Displays all non-font tiles in a labeled grid on screen.
 *
 * GB screen = 20x18 tiles (160x144 px), but background map = 32x32.
 * We use SCY scrolling to pan through a taller layout.
 *
 * Layout (row numbers in tile coords):
 *   Row 0:  "DUNGEON" label
 *   Row 1:  Tiles 0-9 (dungeon: blank,wall,floor,doors,stairs,altar,player,cursor)
 *   Row 2:  "UI + PETS" label
 *   Row 3:  Tiles 10-23 (icons + pets)
 *   Row 4:  blank
 *   Row 5:  "ITEMS" label
 *   Row 6:  Tiles 128-147 (items row 1: weapons, armor, potions)
 *   Row 7:  Tiles 148-167 (items row 2: scrolls, wands, food, tools)
 *   Row 8:  blank
 *   Row 9:  "MONSTERS" label
 *   Row 10: Tiles 192-204 (monsters row 1)
 *   Row 11: Tiles 205-216 (monsters row 2)
 *
 * D-pad up/down scrolls the view.
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include "../res/tiles.h"

/* CGB palette data — 4 colors per palette, RGB555 format */
static const uint16_t bg_palettes[] = {
    /* PAL 0: default greyscale */
    RGB(31,31,31), RGB(20,20,20), RGB(10,10,10), RGB(0,0,0),
    /* PAL 1: green tint for labels */
    RGB(31,31,31), RGB(16,28,16), RGB(8,20,8), RGB(0,8,0),
};

/* Write a text string using font tiles (tile 32 = space, etc.) */
static void put_text(uint8_t x, uint8_t y, const char *s) {
    uint8_t tiles[20];
    uint8_t i = 0;
    while (s[i] && i < 20) {
        tiles[i] = (uint8_t)s[i];  /* ASCII value = tile index */
        i++;
    }
    set_bkg_tiles(x, y, i, 1, tiles);
}

/* Write a row of sequential tile indices starting from 'first' */
static void put_tile_row(uint8_t x, uint8_t y, uint8_t first, uint8_t count) {
    uint8_t tiles[20];
    uint8_t i;
    for (i = 0; i < count && i < 20; i++) {
        tiles[i] = first + i;
    }
    set_bkg_tiles(x, y, count, 1, tiles);
}

void main(void) {
    uint8_t joy;
    uint8_t scroll_y = 0;

    /* --- CGB palette setup --- */
    cpu_fast();
    set_bkg_palette(0, 2, bg_palettes);

    /* --- Load tileset --- */
    DISPLAY_OFF;
    SWITCH_ROM(2);  /* tiles are in bank 2 */
    tiles_load();
    SWITCH_ROM(0);
    DISPLAY_ON;

    /* --- Draw the layout --- */

    /* Row 0: Dungeon label */
    put_text(0, 0, "DUNGEON");
    /* Row 1: Dungeon tiles 0-9 */
    put_tile_row(0, 1, 0, 10);

    /* Row 2: UI label */
    put_text(0, 2, "UI + PETS");
    /* Row 3: Tiles 10-23 */
    put_tile_row(0, 3, 10, 14);

    /* Row 5: Items label */
    put_text(0, 5, "ITEMS: WPN/ARM/POT");
    /* Row 6: Items 128-147 (weapons, armor, potions) */
    put_tile_row(0, 6, 128, 20);
    /* Row 7: Items 148-167 (scrolls, wands, food, tools) */
    put_text(0, 7, "ITEMS: SCR/WND/FOOD");
    put_tile_row(0, 8, 148, 20);

    /* Row 10: Monsters label */
    put_text(0, 10, "MONSTERS 1-13");
    /* Row 11: Monsters 192-204 */
    put_tile_row(0, 11, 192, 13);
    /* Row 12: Monsters label */
    put_text(0, 12, "MONSTERS 14-25");
    /* Row 13: Monsters 205-216 */
    put_tile_row(0, 13, 205, 12);

    /* Row 15: legend */
    put_text(0, 15, "UP/DN TO SCROLL");

    /* --- Main loop: scroll with D-pad --- */
    while (1) {
        wait_vbl_done();
        joy = joypad();

        if (joy & J_UP) {
            if (scroll_y > 0) scroll_y--;
            SCY_REG = scroll_y;
        }
        if (joy & J_DOWN) {
            if (scroll_y < 128) scroll_y++;
            SCY_REG = scroll_y;
        }
    }
}
