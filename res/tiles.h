#ifndef TILES_H
#define TILES_H

#include <stdint.h>
#include <gb/gb.h>

// Dungeon tile indices
#define TILE_BLANK       0
#define TILE_WALL        1
#define TILE_FLOOR       2
#define TILE_DOOR_CLOSED 3
#define TILE_DOOR_OPEN   4
#define TILE_STAIRS_UP   5
#define TILE_STAIRS_DOWN 6
#define TILE_ALTAR       7
#define TILE_PLAYER      8
#define TILE_CURSOR      9
#define TILE_PET_CAT     22
#define TILE_PET_DOG     23

// Base indices for tile categories
#define TILE_FONT_BASE    32   // tile 32 = space, tile 65 = 'A', etc.
#define TILE_MONSTER_BASE 192
#define TILE_ITEM_BASE    128

// Total tiles in the tileset
#define TILE_COUNT        224

BANKREF_EXTERN(tileset_data)
extern const unsigned char tileset_data[];
extern const unsigned int tileset_data_length;

// Load tileset into VRAM (banked — call normally, trampoline handles bank switch)
void tiles_load(void) BANKED;

// Load font tiles (32-127) into VRAM bank 1 for text overlay.
// Caller must set VBK_REG=1 before calling.
void tiles_load_font_bank1(void) BANKED;

#endif
