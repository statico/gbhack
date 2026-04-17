#pragma bank 6

#include "items.h"
#include "dungeon.h"
#include "rng.h"

/* ------------------------------------------------------------------ */
/* Runtime state (WRAM — not affected by bank pragma)                  */
/* ------------------------------------------------------------------ */

Item floor_items[MAX_ITEMS];

uint8_t potion_appearances[6];
uint8_t scroll_appearances[6];
uint8_t identified_potions  = 0;
uint8_t identified_scrolls  = 0;

/* ------------------------------------------------------------------ */
/* Init / shuffle                                                      */
/* ------------------------------------------------------------------ */

void items_init(void) BANKED {
    uint8_t i;
    for (i = 0; i < MAX_ITEMS; i++) {
        floor_items[i].type_id  = 255;
        floor_items[i].quantity = 0;
        floor_items[i].flags    = 0;
        floor_items[i].x        = 255;
        floor_items[i].y        = 255;
    }
    identified_potions = 0;
    identified_scrolls = 0;
    items_shuffle_appearances();
}

void items_shuffle_appearances(void) BANKED {
    uint8_t i, j, tmp;

    /* Initialize identity mapping */
    for (i = 0; i < 6; i++) {
        potion_appearances[i] = i;
        scroll_appearances[i] = i;
    }

    /* Fisher-Yates shuffle for potions */
    for (i = 5; i > 0; i--) {
        j = rng_range(0, i);
        tmp = potion_appearances[i];
        potion_appearances[i] = potion_appearances[j];
        potion_appearances[j] = tmp;
    }

    /* Fisher-Yates shuffle for scrolls */
    for (i = 5; i > 0; i--) {
        j = rng_range(0, i);
        tmp = scroll_appearances[i];
        scroll_appearances[i] = scroll_appearances[j];
        scroll_appearances[j] = tmp;
    }
}

/* ------------------------------------------------------------------ */
/* Floor item management                                               */
/* ------------------------------------------------------------------ */

uint8_t item_at(uint8_t x, uint8_t y) BANKED {
    uint8_t i;
    for (i = 0; i < MAX_ITEMS; i++) {
        if (floor_items[i].type_id != 255 &&
            floor_items[i].x == x &&
            floor_items[i].y == y) {
            return i;
        }
    }
    return 255;
}

uint8_t item_spawn(uint8_t type_id, uint8_t x, uint8_t y, uint8_t qty) BANKED {
    uint8_t i;
    uint8_t cell;

    for (i = 0; i < MAX_ITEMS; i++) {
        if (floor_items[i].type_id == 255) {
            floor_items[i].type_id  = type_id;
            floor_items[i].x        = x;
            floor_items[i].y        = y;
            floor_items[i].quantity  = qty;
            floor_items[i].flags    = IFLAG_ON_GROUND;

            /* Mark the cell */
            cell = dungeon_get_cell(x, y);
            dungeon_set_cell(x, y, cell | CELL_HAS_ITEM);

            return i;
        }
    }
    return 255;  /* no free slots */
}

void item_remove_floor(uint8_t idx) BANKED {
    uint8_t x, y, cell;

    if (idx >= MAX_ITEMS) return;

    x = floor_items[idx].x;
    y = floor_items[idx].y;

    floor_items[idx].type_id  = 255;
    floor_items[idx].quantity = 0;
    floor_items[idx].flags    = 0;
    floor_items[idx].x        = 255;
    floor_items[idx].y        = 255;

    /* Clear CELL_HAS_ITEM only if no other items remain at this position */
    if (item_at(x, y) == 255) {
        cell = dungeon_get_cell(x, y);
        dungeon_set_cell(x, y, cell & ~CELL_HAS_ITEM);
    }
}

/* ------------------------------------------------------------------ */
/* Level item spawning                                                 */
/* ------------------------------------------------------------------ */

static uint8_t pick_random_item(uint8_t level) {
    uint8_t roll;
    uint8_t type_id;

    roll = rng_range(0, 99);

    if (roll < 25) {
        /* Weapon: 25% */
        if (level < 5) {
            type_id = rng_range(0, 1);
        } else if (level < 10) {
            type_id = rng_range(0, 3);
        } else {
            type_id = rng_range(0, 5);
        }
    } else if (roll < 40) {
        /* Armor: 15% */
        if (level < 5) {
            type_id = rng_range(6, 7);
        } else {
            type_id = rng_range(6, 10);
        }
    } else if (roll < 60) {
        /* Potion: 20% */
        type_id = rng_range(11, 16);
    } else if (roll < 75) {
        /* Scroll: 15% */
        type_id = rng_range(17, 22);
    } else if (roll < 85) {
        /* Wand: 10% */
        if (level < 5) {
            type_id = rng_range(23, 25);
        } else if (level < 10) {
            type_id = rng_range(23, 26);
        } else {
            type_id = rng_range(23, 27);
        }
    } else if (roll < 95) {
        /* Food: 10% — skip corpse (30), only ration/apple/tin */
        type_id = rng_range(28, 30);
        if (type_id == 30) type_id = 31;  /* remap corpse -> tin */
    } else {
        /* Tool: 5% */
        type_id = rng_range(33, 39);
    }

    return type_id;
}

static uint8_t item_initial_qty(uint8_t type_id, uint8_t level) {
    uint8_t cat;

    cat = item_types[type_id].category;

    if (type_id == 5) {
        return rng_range(3, 15);
    }
    if (cat == ICAT_GOLD) {
        return rng_range(5, level * 10 + 20);
    }
    if (cat == ICAT_WAND) {
        if (type_id == 27) return rng_range(1, 3);
        if (type_id == 26) return rng_range(4, 8);
        return rng_range(3, 6);
    }
    if (type_id == 34) {  /* Pickaxe: durability */
        return 255;  /* max uint8_t — durability uses */
    }
    return 1;
}

void items_spawn_for_level(uint8_t level) BANKED {
    uint8_t count;
    uint8_t i;
    uint8_t type_id;
    uint8_t qty;
    uint8_t fx, fy;
    uint8_t gold_amount;

    /* Clear all floor items for new level */
    for (i = 0; i < MAX_ITEMS; i++) {
        floor_items[i].type_id = 255;
        floor_items[i].x = 255;
        floor_items[i].y = 255;
        floor_items[i].quantity = 0;
        floor_items[i].flags = 0;
    }

    count = rng_range(3, 6);

    for (i = 0; i < count; i++) {
        type_id = pick_random_item(level);
        qty = item_initial_qty(type_id, level);

        dungeon_find_random_floor(&fx, &fy);
        item_spawn(type_id, fx, fy, qty);
    }

    /* Always place at least 1 food item */
    dungeon_find_random_floor(&fx, &fy);
    item_spawn(rng_range(28, 29), fx, fy, 1);

    /* Always place some gold */
    gold_amount = rng_range(10, level * 15 + 30);
    dungeon_find_random_floor(&fx, &fy);
    item_spawn(32, fx, fy, gold_amount);

    /* Place Amulet of Yendor on deepest level */
    if (level >= MAX_DUNGEON_LEVELS) {
        dungeon_find_random_floor(&fx, &fy);
        item_spawn(35, fx, fy, 1);
    }
}
