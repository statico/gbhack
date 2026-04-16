#ifndef ITEMS_H
#define ITEMS_H
#include "common.h"

extern Item floor_items[MAX_ITEMS];
extern const ItemType item_types[];
extern uint8_t num_item_types;

/* Identification tables (shuffled per run) */
extern uint8_t potion_appearances[6];  /* maps appearance -> real potion ID */
extern uint8_t scroll_appearances[6];  /* maps appearance -> real scroll ID */
extern uint8_t identified_potions;     /* bitmask: which potions are identified */
extern uint8_t identified_scrolls;     /* bitmask: which scrolls are identified */

void items_init(void) BANKED;
void items_shuffle_appearances(void) BANKED;
void items_spawn_for_level(uint8_t level) BANKED;
uint8_t item_at(uint8_t x, uint8_t y) BANKED;  /* returns floor_items index or 255 */
uint8_t item_spawn(uint8_t type_id, uint8_t x, uint8_t y, uint8_t qty) BANKED;
void item_remove_floor(uint8_t idx) BANKED;
const char *item_name(uint8_t type_id);           /* bank 0 — always accessible */
const char *item_appearance_name(uint8_t type_id); /* bank 0 — always accessible */

#endif
