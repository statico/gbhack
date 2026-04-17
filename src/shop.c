#pragma bank 5

#include "shop.h"
#include "dungeon.h"
#include "items.h"
#include "monsters.h"
#include "player.h"
#include "rng.h"
#include "ui.h"

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

uint16_t shop_debt;
uint8_t shop_active;
uint8_t shop_room_index;

/* Track which item types were picked up and how many, for debt bookkeeping */
static uint8_t debt_type_ids[MAX_SHOP_DEBT_ITEMS];
static uint8_t debt_count;

/* Shopkeeper monster index in this room (255 = none) */
static uint8_t shopkeeper_idx;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Find the shopkeeper monster in the current shop room */
static uint8_t find_shopkeeper(void) {
    uint8_t i;
    uint8_t ri;
    for (i = 0; i < MAX_MONSTERS; i++) {
        if (monsters[i].active && monsters[i].type_id == 22) {
            ri = dungeon_find_room_at(monsters[i].x, monsters[i].y);
            if (ri == shop_room_index) {
                return i;
            }
        }
    }
    return 255;
}

/* Shopkeeper item types for stocking: weapons, armor, potions, scrolls,
 * wands, food, tools -- no gold or amulet */
static uint8_t pick_shop_item(void) {
    uint8_t roll;
    uint8_t type_id;

    roll = rng_range(0, 99);

    if (roll < 20) {
        /* Weapon: 20% */
        type_id = rng_range(0, 5);
    } else if (roll < 40) {
        /* Armor: 20% */
        type_id = rng_range(6, 10);
    } else if (roll < 60) {
        /* Potion: 20% */
        type_id = rng_range(11, 16);
    } else if (roll < 75) {
        /* Scroll: 15% */
        type_id = rng_range(17, 22);
    } else if (roll < 85) {
        /* Wand: 10% */
        type_id = rng_range(23, 27);
    } else if (roll < 95) {
        /* Food: 10% — skip corpse (30) */
        type_id = rng_range(28, 30);
        if (type_id == 30) type_id = 31;
    } else {
        /* Tool: 5% */
        type_id = rng_range(33, 39);
    }

    return type_id;
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

void shop_init(void) BANKED {
    uint8_t i;

    shop_debt = 0;
    shop_active = 0;
    shop_room_index = 255;
    shopkeeper_idx = 255;
    debt_count = 0;

    for (i = 0; i < MAX_SHOP_DEBT_ITEMS; i++) {
        debt_type_ids[i] = 255;
    }
}

/* ------------------------------------------------------------------ */
/* Enter / leave detection                                             */
/* ------------------------------------------------------------------ */

void shop_check_enter(void) BANKED {
    uint8_t ri;
    uint8_t was_active;

    was_active = shop_active;
    ri = dungeon_find_room_at(player.x, player.y);

    if (ri != 255 && dungeon_rooms[ri].type == 1) {
        /* Player is inside a shop room */
        if (!was_active) {
            /* Just entered */
            shop_active = 1;
            shop_room_index = ri;
            shopkeeper_idx = find_shopkeeper();

            if (player.shopkeeper_hostile) {
                ui_message("The shopkeeper");
                ui_message("attacks!");
                /* Make shopkeeper aggressive immediately */
                if (shopkeeper_idx != 255) {
                    monsters[shopkeeper_idx].status &= ~MSTAT_PEACEFUL;
                }
            } else {
                ui_message("Welcome to the");
                ui_message("shop!");
            }
        }
    } else {
        /* Player is outside shop */
        if (was_active) {
            /* Just left -- check debt */
            if (shop_debt > 0) {
                shop_try_leave();
            } else {
                shop_active = 0;
                shop_room_index = 255;
                shopkeeper_idx = 255;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Pickup / drop in shop                                               */
/* ------------------------------------------------------------------ */

void shop_on_pickup(uint8_t item_type_id) BANKED {
    uint16_t price;

    if (!shop_active) return;

    /* Gold pickups are free */
    if (item_types[item_type_id].category == ICAT_GOLD) return;

    price = (uint16_t)item_types[item_type_id].base_price;
    if (price == 0) price = 1;

    shop_debt += price;

    /* Track for drop-back */
    if (debt_count < MAX_SHOP_DEBT_ITEMS) {
        debt_type_ids[debt_count] = item_type_id;
        debt_count++;
    }

    ui_message("That will be");
    /* Build price string manually for SDCC compat */
    {
        char buf[MSG_MAX_LEN];
        uint8_t pos;
        uint16_t val;
        uint8_t digits[5];
        uint8_t dcount;
        uint8_t d;

        val = price;
        dcount = 0;
        if (val == 0) {
            digits[0] = 0;
            dcount = 1;
        } else {
            while (val > 0 && dcount < 5) {
                digits[dcount] = (uint8_t)(val % 10);
                val /= 10;
                dcount++;
            }
        }
        pos = 0;
        /* Print digits in reverse order */
        for (d = dcount; d > 0; d--) {
            buf[pos] = '0' + digits[d - 1];
            pos++;
        }
        buf[pos++] = 'g';
        buf[pos++] = 'p';
        buf[pos++] = '.';
        buf[pos] = '\0';
        ui_message(buf);
    }
}

void shop_on_drop(uint8_t item_type_id) BANKED {
    uint16_t price;
    uint8_t i;

    if (!shop_active) return;

    /* Gold drops are free */
    if (item_types[item_type_id].category == ICAT_GOLD) return;

    /* Check if this item type was picked up from the shop */
    for (i = 0; i < debt_count; i++) {
        if (debt_type_ids[i] == item_type_id) {
            /* Found -- remove from debt tracking */
            price = (uint16_t)item_types[item_type_id].base_price;
            if (price == 0) price = 1;

            if (price > shop_debt) {
                shop_debt = 0;
            } else {
                shop_debt -= price;
            }

            /* Shift remaining entries down */
            debt_count--;
            for (; i < debt_count; i++) {
                debt_type_ids[i] = debt_type_ids[i + 1];
            }
            debt_type_ids[debt_count] = 255;

            ui_message("You put it back.");
            return;
        }
    }

    /* Item was not from this shop -- shopkeeper buys it */
    price = (uint16_t)item_types[item_type_id].base_price / 2;
    if (price == 0) price = 1;

    player.gold += price;
    ui_message("Sold!");
    {
        char buf[MSG_MAX_LEN];
        uint8_t pos;
        uint16_t val;
        uint8_t digits[5];
        uint8_t dcount;
        uint8_t d;

        val = price;
        dcount = 0;
        if (val == 0) {
            digits[0] = 0;
            dcount = 1;
        } else {
            while (val > 0 && dcount < 5) {
                digits[dcount] = (uint8_t)(val % 10);
                val /= 10;
                dcount++;
            }
        }
        pos = 0;
        buf[pos++] = '+';
        for (d = dcount; d > 0; d--) {
            buf[pos] = '0' + digits[d - 1];
            pos++;
        }
        buf[pos++] = 'g';
        buf[pos++] = 'p';
        buf[pos] = '\0';
        ui_message(buf);
    }
}

/* ------------------------------------------------------------------ */
/* Leaving the shop                                                    */
/* ------------------------------------------------------------------ */

uint8_t shop_can_leave(void) BANKED {
    if (shop_debt == 0) return 1;
    return 0;
}

void shop_try_leave(void) BANKED {
    if (shop_debt == 0) {
        shop_active = 0;
        shop_room_index = 255;
        shopkeeper_idx = 255;
        return;
    }

    ui_message("You owe gold!");
    ui_message("Pay up!");

    /* Anger the shopkeeper */
    shopkeeper_idx = find_shopkeeper();
    if (shopkeeper_idx != 255) {
        monsters[shopkeeper_idx].status &= ~MSTAT_PEACEFUL;
    }
}

/* ------------------------------------------------------------------ */
/* Paying debt                                                         */
/* ------------------------------------------------------------------ */

void shop_pay(void) BANKED {
    uint8_t i;

    if (shop_debt == 0) {
        ui_message("You owe nothing.");
        return;
    }

    if (player.gold >= shop_debt) {
        player.gold -= shop_debt;
        shop_debt = 0;
        debt_count = 0;
        for (i = 0; i < MAX_SHOP_DEBT_ITEMS; i++) {
            debt_type_ids[i] = 255;
        }
        ui_message("Thank you for");
        ui_message("your business!");
    } else {
        ui_message("You don't have");
        ui_message("enough gold!");
    }
}

/* ------------------------------------------------------------------ */
/* Stock the shop room with items and a shopkeeper                     */
/* ------------------------------------------------------------------ */

void shop_stock_room(uint8_t room_idx) BANKED {
    Room *r;
    uint8_t count;
    uint8_t i;
    uint8_t type_id;
    uint8_t sx, sy;
    uint8_t terrain;
    uint8_t attempts;
    uint8_t door_x, door_y;
    uint8_t best_x, best_y;
    uint8_t x, y;
    uint8_t found_door;
    uint8_t sk_idx;

    if (room_idx >= dungeon_num_rooms) return;

    r = &dungeon_rooms[room_idx];

    /* Spawn 4-6 random items on shop floor tiles */
    count = rng_range(4, 6);

    for (i = 0; i < count; i++) {
        type_id = pick_shop_item();

        /* Find a random TERRAIN_SHOP_FLOOR tile in this room */
        for (attempts = 0; attempts < 30; attempts++) {
            sx = rng_range(r->x, r->x + r->w - 1);
            sy = rng_range(r->y, r->y + r->h - 1);
            terrain = CELL_TERRAIN(dungeon_get_cell(sx, sy));
            if (terrain == TERRAIN_SHOP_FLOOR) {
                /* Don't stack items */
                if (!(dungeon_get_cell(sx, sy) & CELL_HAS_ITEM)) {
                    item_spawn(type_id, sx, sy, 1);
                    break;
                }
            }
        }
    }

    /* Find a door adjacent to the shop room for shopkeeper placement */
    found_door = 0;
    door_x = r->x;
    door_y = r->y;

    /* Scan the border of the room for a door or passable tile */
    /* Check top and bottom edges */
    for (x = r->x; x < r->x + r->w; x++) {
        /* One tile above room */
        if (r->y > 0) {
            terrain = CELL_TERRAIN(dungeon_get_cell(x, r->y - 1));
            if (terrain == TERRAIN_DOOR_OPEN || terrain == TERRAIN_DOOR_CLOSED ||
                terrain == TERRAIN_CORRIDOR) {
                door_x = x;
                door_y = r->y - 1;
                found_door = 1;
                break;
            }
        }
        /* One tile below room */
        if (r->y + r->h < MAP_HEIGHT) {
            terrain = CELL_TERRAIN(dungeon_get_cell(x, r->y + r->h));
            if (terrain == TERRAIN_DOOR_OPEN || terrain == TERRAIN_DOOR_CLOSED ||
                terrain == TERRAIN_CORRIDOR) {
                door_x = x;
                door_y = r->y + r->h;
                found_door = 1;
                break;
            }
        }
    }

    if (!found_door) {
        /* Check left and right edges */
        for (y = r->y; y < r->y + r->h; y++) {
            if (r->x > 0) {
                terrain = CELL_TERRAIN(dungeon_get_cell(r->x - 1, y));
                if (terrain == TERRAIN_DOOR_OPEN || terrain == TERRAIN_DOOR_CLOSED ||
                    terrain == TERRAIN_CORRIDOR) {
                    door_x = r->x - 1;
                    door_y = y;
                    found_door = 1;
                    break;
                }
            }
            if (r->x + r->w < MAP_WIDTH) {
                terrain = CELL_TERRAIN(dungeon_get_cell(r->x + r->w, y));
                if (terrain == TERRAIN_DOOR_OPEN || terrain == TERRAIN_DOOR_CLOSED ||
                    terrain == TERRAIN_CORRIDOR) {
                    door_x = r->x + r->w;
                    door_y = y;
                    found_door = 1;
                    break;
                }
            }
        }
    }

    /* Place shopkeeper just inside the room near the door */
    best_x = r->x + 1;
    best_y = r->y + 1;

    if (found_door) {
        /* Find shop floor tile closest to the door */
        {
            uint8_t bx, by;
            uint8_t best_dist;
            uint8_t dist;

            best_dist = 255;
            for (by = r->y; by < r->y + r->h; by++) {
                for (bx = r->x; bx < r->x + r->w; bx++) {
                    terrain = CELL_TERRAIN(dungeon_get_cell(bx, by));
                    if (terrain != TERRAIN_SHOP_FLOOR) continue;
                    if (dungeon_get_cell(bx, by) & CELL_HAS_ITEM) continue;
                    if (dungeon_get_cell(bx, by) & CELL_HAS_MONSTER) continue;
                    dist = ABS((int8_t)bx - (int8_t)door_x) +
                           ABS((int8_t)by - (int8_t)door_y);
                    if (dist < best_dist) {
                        best_dist = dist;
                        best_x = bx;
                        best_y = by;
                    }
                }
            }
        }
    }

    /* Spawn the shopkeeper */
    monster_spawn(22, best_x, best_y);

    /* If player has angered shopkeepers globally, make this one hostile */
    if (player.shopkeeper_hostile) {
        sk_idx = monster_at(best_x, best_y);
        if (sk_idx != 255) {
            monsters[sk_idx].status &= ~MSTAT_PEACEFUL;
        }
    }
}
