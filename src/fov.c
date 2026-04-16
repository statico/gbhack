#pragma bank 3

#include "fov.h"
#include "dungeon.h"

/* Visibility bitmap: 1 bit per map cell.
 * Index = y * MAP_WIDTH + x.
 * Byte = index / 8, bit = index % 8. */
uint8_t fov_visible[150];

/* Max ray distance for corridor vision */
#define FOV_RAY_MAX 8

void fov_clear(void) BANKED {
    uint8_t i;
    for (i = 0; i < 150; i++) {
        fov_visible[i] = 0;
    }
}

static void fov_mark(uint8_t x, uint8_t y) {
    uint16_t idx;
    uint8_t byte_idx;
    uint8_t bit_idx;
    uint8_t cell;

    if (x >= MAP_WIDTH || y >= MAP_HEIGHT) return;

    idx = (uint16_t)y * MAP_WIDTH + x;
    byte_idx = (uint8_t)(idx >> 3);
    bit_idx = (uint8_t)(idx & 7);
    fov_visible[byte_idx] |= (1 << bit_idx);

    /* Mark the cell as seen in the dungeon map */
    cell = dungeon_get_cell(x, y);
    if (!(cell & CELL_SEEN)) {
        dungeon_set_cell(x, y, cell | CELL_SEEN);
    }
}

uint8_t fov_is_visible(uint8_t x, uint8_t y) BANKED {
    uint16_t idx;
    uint8_t byte_idx;
    uint8_t bit_idx;

    if (x >= MAP_WIDTH || y >= MAP_HEIGHT) return 0;

    idx = (uint16_t)y * MAP_WIDTH + x;
    byte_idx = (uint8_t)(idx >> 3);
    bit_idx = (uint8_t)(idx & 7);
    return (fov_visible[byte_idx] >> bit_idx) & 1;
}

/* Cast a ray in direction (dx, dy) from origin.
 * Marks cells visible until hitting a wall or reaching max range.
 * Walls are marked visible (you see the wall itself) but stop the ray. */
static void fov_cast_ray(uint8_t ox, uint8_t oy, int8_t dx, int8_t dy) {
    uint8_t cx, cy;
    uint8_t step;
    uint8_t terrain;

    cx = ox;
    cy = oy;
    for (step = 0; step < FOV_RAY_MAX; step++) {
        cx = (uint8_t)((int8_t)cx + dx);
        cy = (uint8_t)((int8_t)cy + dy);

        if (cx >= MAP_WIDTH || cy >= MAP_HEIGHT) return;

        fov_mark(cx, cy);

        terrain = CELL_TERRAIN(dungeon_get_cell(cx, cy));
        if (terrain == TERRAIN_WALL || terrain == TERRAIN_DOOR_CLOSED) {
            return; /* Wall stops the ray */
        }
    }
}

void fov_calculate(uint8_t px, uint8_t py) BANKED {
    uint8_t room_idx;
    uint8_t rx, ry;
    int8_t dx, dy;

    fov_clear();

    /* Always mark 3x3 area around player */
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            rx = (uint8_t)((int8_t)px + dx);
            ry = (uint8_t)((int8_t)py + dy);
            if (rx < MAP_WIDTH && ry < MAP_HEIGHT) {
                fov_mark(rx, ry);
            }
        }
    }

    /* Check if player is in a lit room */
    room_idx = dungeon_find_room_at(px, py);
    if (room_idx != 255) {
        Room *r;
        uint8_t x, y;
        r = &dungeon_rooms[room_idx];
        if (r->lit) {
            /* Mark entire room visible */
            for (y = r->y; y < r->y + r->h; y++) {
                for (x = r->x; x < r->x + r->w; x++) {
                    fov_mark(x, y);
                }
            }
            /* Also mark the walls surrounding the room (1 tile border) */
            /* Top and bottom walls */
            if (r->y > 0) {
                for (rx = (r->x > 0) ? (r->x - 1) : 0;
                     rx <= r->x + r->w && rx < MAP_WIDTH; rx++) {
                    fov_mark(rx, r->y - 1);
                }
            }
            if (r->y + r->h < MAP_HEIGHT) {
                for (rx = (r->x > 0) ? (r->x - 1) : 0;
                     rx <= r->x + r->w && rx < MAP_WIDTH; rx++) {
                    fov_mark(rx, r->y + r->h);
                }
            }
            /* Left and right walls */
            if (r->x > 0) {
                for (ry = r->y; ry < r->y + r->h && ry < MAP_HEIGHT; ry++) {
                    fov_mark(r->x - 1, ry);
                }
            }
            if (r->x + r->w < MAP_WIDTH) {
                for (ry = r->y; ry < r->y + r->h && ry < MAP_HEIGHT; ry++) {
                    fov_mark(r->x + r->w, ry);
                }
            }
        }
    }

    /* Cast rays in 8 directions for corridor visibility */
    fov_cast_ray(px, py,  0, -1); /* N */
    fov_cast_ray(px, py,  0,  1); /* S */
    fov_cast_ray(px, py,  1,  0); /* E */
    fov_cast_ray(px, py, -1,  0); /* W */
    fov_cast_ray(px, py,  1, -1); /* NE */
    fov_cast_ray(px, py, -1, -1); /* NW */
    fov_cast_ray(px, py,  1,  1); /* SE */
    fov_cast_ray(px, py, -1,  1); /* SW */
}
