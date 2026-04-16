#ifndef DUNGEON_H
#define DUNGEON_H

#include "common.h"

/* The current level map */
extern uint8_t dungeon_map[MAP_WIDTH * MAP_HEIGHT];
extern Room dungeon_rooms[MAX_ROOMS];
extern uint8_t dungeon_num_rooms;
extern uint8_t stairs_up_x, stairs_up_y;
extern uint8_t stairs_down_x, stairs_down_y;

void dungeon_generate(uint8_t level) BANKED;
uint8_t dungeon_get_cell(uint8_t x, uint8_t y) BANKED;
void dungeon_set_cell(uint8_t x, uint8_t y, uint8_t value) BANKED;
uint8_t dungeon_is_passable(uint8_t x, uint8_t y) BANKED;
void dungeon_open_door(uint8_t x, uint8_t y) BANKED;
void dungeon_find_random_floor(uint8_t *out_x, uint8_t *out_y) BANKED;
uint8_t dungeon_find_room_at(uint8_t x, uint8_t y) BANKED; /* returns room index or 255 */

#endif
