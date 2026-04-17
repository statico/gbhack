#pragma bank 3

#include "dungeon.h"
#include "rng.h"

uint8_t dungeon_map[MAP_WIDTH * MAP_HEIGHT];
Room dungeon_rooms[MAX_ROOMS];
uint8_t dungeon_num_rooms;
uint8_t stairs_up_x, stairs_up_y;
uint8_t stairs_down_x, stairs_down_y;

uint8_t dungeon_get_cell(uint8_t x, uint8_t y) BANKED {
    if (x >= MAP_WIDTH || y >= MAP_HEIGHT) return TERRAIN_WALL;
    return dungeon_map[(uint16_t)y * MAP_WIDTH + x];
}

void dungeon_set_cell(uint8_t x, uint8_t y, uint8_t value) BANKED {
    if (x >= MAP_WIDTH || y >= MAP_HEIGHT) return;
    dungeon_map[(uint16_t)y * MAP_WIDTH + x] = value;
}

uint8_t dungeon_is_passable(uint8_t x, uint8_t y) BANKED {
    uint8_t cell;
    uint8_t terrain;
    if (x >= MAP_WIDTH || y >= MAP_HEIGHT) return 0;
    cell = dungeon_get_cell(x, y);
    terrain = CELL_TERRAIN(cell);
    if (terrain == TERRAIN_WALL || terrain == TERRAIN_DOOR_CLOSED) return 0;
    return 1;
}

void dungeon_open_door(uint8_t x, uint8_t y) BANKED {
    uint8_t cell;
    uint8_t terrain;
    cell = dungeon_get_cell(x, y);
    terrain = CELL_TERRAIN(cell);
    if (terrain == TERRAIN_DOOR_CLOSED) {
        /* Preserve flags (bits 4-7), replace terrain with open door */
        dungeon_set_cell(x, y, (cell & 0xF0) | TERRAIN_DOOR_OPEN);
    }
}

void dungeon_find_random_floor(uint8_t *out_x, uint8_t *out_y) BANKED {
    uint8_t x, y, terrain;
    uint8_t attempts;
    for (attempts = 0; attempts < 200; attempts++) {
        x = rng_range(1, MAP_WIDTH - 2);
        y = rng_range(1, MAP_HEIGHT - 2);
        terrain = CELL_TERRAIN(dungeon_get_cell(x, y));
        if (terrain == TERRAIN_FLOOR) {
            *out_x = x;
            *out_y = y;
            return;
        }
    }
    /* Fallback: linear scan */
    for (y = 1; y < MAP_HEIGHT - 1; y++) {
        for (x = 1; x < MAP_WIDTH - 1; x++) {
            terrain = CELL_TERRAIN(dungeon_get_cell(x, y));
            if (terrain == TERRAIN_FLOOR) {
                *out_x = x;
                *out_y = y;
                return;
            }
        }
    }
    /* Should never reach here if generation worked */
    *out_x = MAP_WIDTH / 2;
    *out_y = MAP_HEIGHT / 2;
}

uint8_t dungeon_find_room_at(uint8_t x, uint8_t y) BANKED {
    uint8_t i;
    Room *r;
    for (i = 0; i < dungeon_num_rooms; i++) {
        r = &dungeon_rooms[i];
        if (x >= r->x && x < r->x + r->w &&
            y >= r->y && y < r->y + r->h) {
            return i;
        }
    }
    return 255;
}

/* ---- Internal helpers ---- */

static void fill_map(uint8_t value) {
    uint16_t i;
    for (i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++) {
        dungeon_map[i] = value;
    }
}

static uint8_t rooms_overlap(uint8_t ax, uint8_t ay, uint8_t aw, uint8_t ah,
                              uint8_t bx, uint8_t by, uint8_t bw, uint8_t bh) {
    /* Check with 1-tile margin around room b */
    if (ax + aw <= bx - 1) return 0;
    if (bx + bw <= ax - 1) return 0;
    if (ay + ah <= by - 1) return 0;
    if (by + bh <= ay - 1) return 0;
    return 1;
}

static void carve_room(uint8_t room_idx) {
    Room *r;
    uint8_t x, y;
    uint8_t flags;
    r = &dungeon_rooms[room_idx];
    flags = r->lit ? CELL_LIT : 0;
    for (y = r->y; y < r->y + r->h; y++) {
        for (x = r->x; x < r->x + r->w; x++) {
            dungeon_set_cell(x, y, TERRAIN_FLOOR | flags);
        }
    }
}

static void place_rooms(void) {
    uint8_t target_rooms;
    uint8_t attempts;
    uint8_t rx, ry, rw, rh;
    uint8_t i;
    uint8_t overlap;
    uint8_t lit;

    target_rooms = rng_range(4, MAX_ROOMS);
    dungeon_num_rooms = 0;

    for (attempts = 0; attempts < 80 && dungeon_num_rooms < target_rooms; attempts++) {
        rw = rng_range(4, 10);
        rh = rng_range(4, 8);
        rx = rng_range(1, MAP_WIDTH - rw - 1);
        ry = rng_range(1, MAP_HEIGHT - rh - 1);

        /* Check overlap with all existing rooms */
        overlap = 0;
        for (i = 0; i < dungeon_num_rooms; i++) {
            if (rooms_overlap(rx, ry, rw, rh,
                              dungeon_rooms[i].x, dungeon_rooms[i].y,
                              dungeon_rooms[i].w, dungeon_rooms[i].h)) {
                overlap = 1;
                break;
            }
        }
        if (overlap) continue;

        /* 80% chance lit */
        lit = (rng_range(1, 100) <= 80) ? 1 : 0;

        dungeon_rooms[dungeon_num_rooms].x = rx;
        dungeon_rooms[dungeon_num_rooms].y = ry;
        dungeon_rooms[dungeon_num_rooms].w = rw;
        dungeon_rooms[dungeon_num_rooms].h = rh;
        dungeon_rooms[dungeon_num_rooms].type = 0;
        dungeon_rooms[dungeon_num_rooms].lit = lit;

        carve_room(dungeon_num_rooms);
        dungeon_num_rooms++;
    }
}

static void carve_h_corridor(uint8_t x1, uint8_t x2, uint8_t y) {
    uint8_t x, start, end;
    uint8_t terrain;
    start = (x1 < x2) ? x1 : x2;
    end = (x1 < x2) ? x2 : x1;
    for (x = start; x <= end; x++) {
        terrain = CELL_TERRAIN(dungeon_get_cell(x, y));
        if (terrain == TERRAIN_WALL) {
            dungeon_set_cell(x, y, TERRAIN_CORRIDOR);
        }
    }
}

static void carve_v_corridor(uint8_t y1, uint8_t y2, uint8_t x) {
    uint8_t y, start, end;
    uint8_t terrain;
    start = (y1 < y2) ? y1 : y2;
    end = (y1 < y2) ? y2 : y1;
    for (y = start; y <= end; y++) {
        terrain = CELL_TERRAIN(dungeon_get_cell(x, y));
        if (terrain == TERRAIN_WALL) {
            dungeon_set_cell(x, y, TERRAIN_CORRIDOR);
        }
    }
}

static void connect_rooms(void) {
    uint8_t i;
    uint8_t x1, y1, x2, y2;
    Room *r1, *r2;

    for (i = 0; i + 1 < dungeon_num_rooms; i++) {
        r1 = &dungeon_rooms[i];
        r2 = &dungeon_rooms[i + 1];

        /* Pick random point inside each room */
        x1 = rng_range(r1->x + 1, r1->x + r1->w - 2);
        y1 = rng_range(r1->y + 1, r1->y + r1->h - 2);
        x2 = rng_range(r2->x + 1, r2->x + r2->w - 2);
        y2 = rng_range(r2->y + 1, r2->y + r2->h - 2);

        /* L-shaped corridor: randomly choose horizontal-first or vertical-first */
        if (rng_range(0, 1)) {
            carve_h_corridor(x1, x2, y1);
            carve_v_corridor(y1, y2, x2);
        } else {
            carve_v_corridor(y1, y2, x1);
            carve_h_corridor(x1, x2, y2);
        }
    }
}

static void place_doors(void) {
    uint8_t x, y;
    uint8_t terrain;
    uint8_t n_terrain, s_terrain, e_terrain, w_terrain;
    uint8_t ns_has_floor, ew_has_floor;
    uint8_t ns_both_wall, ew_both_wall;

    /*
     * Place doors at pinch points: corridor cells where one axis
     * connects to room floor and the perpendicular axis is walled.
     * This prevents long runs of doors along parallel corridors.
     */
    for (y = 1; y < MAP_HEIGHT - 1; y++) {
        for (x = 1; x < MAP_WIDTH - 1; x++) {
            terrain = CELL_TERRAIN(dungeon_get_cell(x, y));
            if (terrain != TERRAIN_CORRIDOR) continue;

            n_terrain = CELL_TERRAIN(dungeon_get_cell(x, y - 1));
            s_terrain = CELL_TERRAIN(dungeon_get_cell(x, y + 1));
            e_terrain = CELL_TERRAIN(dungeon_get_cell(x + 1, y));
            w_terrain = CELL_TERRAIN(dungeon_get_cell(x - 1, y));

            ns_has_floor = (n_terrain == TERRAIN_FLOOR ||
                            s_terrain == TERRAIN_FLOOR);
            ew_has_floor = (e_terrain == TERRAIN_FLOOR ||
                            w_terrain == TERRAIN_FLOOR);
            ns_both_wall = (n_terrain == TERRAIN_WALL &&
                            s_terrain == TERRAIN_WALL);
            ew_both_wall = (e_terrain == TERRAIN_WALL &&
                            w_terrain == TERRAIN_WALL);

            /*
             * Valid door: floor on one axis, wall on the other.
             * N/S has floor + E/W both wall = horizontal room edge
             * E/W has floor + N/S both wall = vertical room edge
             */
            if ((ns_has_floor && ew_both_wall) ||
                (ew_has_floor && ns_both_wall)) {
                dungeon_set_cell(x, y, TERRAIN_DOOR_CLOSED);
            }
        }
    }
}

static void place_stairs(uint8_t level) {
    uint8_t x, y;
    uint8_t safety;

    stairs_up_x = 0;
    stairs_up_y = 0;
    stairs_down_x = 0;
    stairs_down_y = 0;

    /* Up stairs (skip for level 1 -- this is the topmost level) */
    if (level > 1) {
        dungeon_find_random_floor(&x, &y);
        stairs_up_x = x;
        stairs_up_y = y;
        dungeon_set_cell(x, y, TERRAIN_STAIRS_UP | CELL_LIT);
    } else {
        /* Level 1: no up stairs, but set spawn point to a valid floor tile */
        dungeon_find_random_floor(&x, &y);
        stairs_up_x = x;
        stairs_up_y = y;
    }

    /* Down stairs or final altar */
    dungeon_find_random_floor(&x, &y);
    /* Make sure down stairs is not on same tile as up stairs */
    if (level > 1) {
        for (safety = 0; safety < 50; safety++) {
            if (x != stairs_up_x || y != stairs_up_y) break;
            dungeon_find_random_floor(&x, &y);
        }
    }

    if (level >= MAX_DUNGEON_LEVELS) {
        /* Final level: place Amulet altar instead of down stairs */
        stairs_down_x = x;
        stairs_down_y = y;
        dungeon_set_cell(x, y, TERRAIN_ALTAR | CELL_LIT);
    } else {
        stairs_down_x = x;
        stairs_down_y = y;
        dungeon_set_cell(x, y, TERRAIN_STAIRS_DOWN | CELL_LIT);
    }
}

static void place_special_rooms(uint8_t level) {
    uint8_t i;
    Room *r;
    uint8_t x, y, flags;

    /* Shop: level 3-10, 20% chance */
    if (level >= 3 && level <= 10 && dungeon_num_rooms >= 2) {
        if (rng_range(1, 100) <= 20) {
            /* Pick a room (not the first one, to keep stairs room normal) */
            i = rng_range(1, dungeon_num_rooms - 1);
            r = &dungeon_rooms[i];
            r->type = 1;
            flags = r->lit ? CELL_LIT : 0;
            for (y = r->y; y < r->y + r->h; y++) {
                for (x = r->x; x < r->x + r->w; x++) {
                    dungeon_set_cell(x, y, TERRAIN_SHOP_FLOOR | flags);
                }
            }
        }
    }

    /* Altar room: level 5-12, 15% chance */
    if (level >= 5 && level <= 12 && dungeon_num_rooms >= 3) {
        if (rng_range(1, 100) <= 15) {
            /* Pick a room that isn't already special */
            for (i = 1; i < dungeon_num_rooms; i++) {
                if (dungeon_rooms[i].type == 0) {
                    r = &dungeon_rooms[i];
                    r->type = 2;
                    /* Place altar on a random floor tile inside the room */
                    x = rng_range(r->x + 1, r->x + r->w - 2);
                    y = rng_range(r->y + 1, r->y + r->h - 2);
                    flags = r->lit ? CELL_LIT : 0;
                    dungeon_set_cell(x, y, TERRAIN_ALTAR | flags);
                    break;
                }
            }
        }
    }
}

void dungeon_generate(uint8_t level) BANKED {
    /* Step 1: Fill with walls */
    fill_map(TERRAIN_WALL);

    /* Step 2: Place rooms */
    place_rooms();

    /* Step 3: Connect rooms with corridors */
    connect_rooms();

    /* Step 4: Place doors at room-corridor transitions */
    place_doors();

    /* Step 5: Place stairs */
    place_stairs(level);

    /* Step 6: Special rooms (shop, altar) */
    place_special_rooms(level);
}
