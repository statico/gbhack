#pragma bank 5

#include "inventory.h"
#include "items.h"
#include "player.h"
#include "dungeon.h"
#include "monsters.h"
#include "rng.h"
#include "ui.h"

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

Item inventory[MAX_INVENTORY];
uint8_t inventory_count = 0;

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

void inventory_init(void) BANKED {
    uint8_t i;
    for (i = 0; i < MAX_INVENTORY; i++) {
        inventory[i].type_id  = 255;
        inventory[i].quantity = 0;
        inventory[i].flags    = 0;
        inventory[i].x        = 255;
        inventory[i].y        = 255;
    }
    inventory_count = 0;
}

/* ------------------------------------------------------------------ */
/* Add / remove                                                        */
/* ------------------------------------------------------------------ */

uint8_t inventory_add(uint8_t type_id, uint8_t qty, uint8_t flags) BANKED {
    uint8_t i;
    uint8_t cat;

    cat = item_types[type_id].category;

    /* Try to stack with existing item of same type */
    if (cat == ICAT_GOLD || type_id == 5 /* Arrow */ || cat == ICAT_FOOD) {
        for (i = 0; i < MAX_INVENTORY; i++) {
            if (inventory[i].type_id == type_id) {
                inventory[i].quantity += qty;
                return i;
            }
        }
    }

    /* Find an empty slot */
    for (i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].type_id == 255) {
            inventory[i].type_id  = type_id;
            inventory[i].quantity = qty;
            inventory[i].flags    = flags & ~IFLAG_ON_GROUND;
            inventory[i].x        = 255;
            inventory[i].y        = 255;
            inventory_count++;
            return i;
        }
    }

    return 255;  /* inventory full */
}

void inventory_remove(uint8_t slot) BANKED {
    if (slot >= MAX_INVENTORY) return;
    if (inventory[slot].type_id == 255) return;

    inventory[slot].type_id  = 255;
    inventory[slot].quantity = 0;
    inventory[slot].flags    = 0;
    inventory[slot].x        = 255;
    inventory[slot].y        = 255;

    if (inventory_count > 0) {
        inventory_count--;
    }
}

uint8_t inventory_find(uint8_t type_id) BANKED {
    uint8_t i;
    for (i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].type_id == type_id) {
            return i;
        }
    }
    return 255;
}

/* ------------------------------------------------------------------ */
/* Pickup                                                              */
/* ------------------------------------------------------------------ */

void inventory_pickup(void) BANKED {
    uint8_t idx;
    uint8_t slot;
    uint8_t type_id;
    uint8_t qty;
    uint8_t flags;

    idx = item_at(player.x, player.y);
    if (idx == 255) {
        ui_message("Nothing here.");
        return;
    }

    type_id = floor_items[idx].type_id;
    qty     = floor_items[idx].quantity;
    flags   = floor_items[idx].flags;

    /* Gold goes directly to player gold count */
    if (item_types[type_id].category == ICAT_GOLD) {
        player.gold += qty;
        item_remove_floor(idx);
        ui_message("You pick up gold.");
        return;
    }

    slot = inventory_add(type_id, qty, flags);
    if (slot == 255) {
        ui_message("Your pack is full!");
        return;
    }

    item_remove_floor(idx);
    ui_message(item_appearance_name(type_id));
}

/* ------------------------------------------------------------------ */
/* Drop                                                                */
/* ------------------------------------------------------------------ */

/* Recompute player.ac from currently equipped armor. */
static void refresh_armor_ac(void) {
    player.ac = inventory_get_armor_ac();
}

void inventory_drop(uint8_t slot) BANKED {
    uint8_t type_id;
    uint8_t qty;
    uint8_t spawned;
    uint8_t was_equipped_armor;

    if (slot >= MAX_INVENTORY) return;
    if (inventory[slot].type_id == 255) return;

    /* Cannot drop cursed equipped items */
    if ((inventory[slot].flags & IFLAG_EQUIPPED) &&
        (inventory[slot].flags & IFLAG_CURSED)) {
        ui_message("It's cursed!");
        return;
    }

    type_id = inventory[slot].type_id;
    was_equipped_armor = (inventory[slot].flags & IFLAG_EQUIPPED) &&
                         (item_types[type_id].category == ICAT_ARMOR);

    /* Unequip first if equipped */
    if (inventory[slot].flags & IFLAG_EQUIPPED) {
        inventory[slot].flags &= ~IFLAG_EQUIPPED;
        if (was_equipped_armor) {
            refresh_armor_ac();
        }
    }

    qty     = inventory[slot].quantity;

    spawned = item_spawn(type_id, player.x, player.y, qty);
    if (spawned == 255) {
        ui_message("No room here.");
        return;
    }

    /* Preserve BUC and identification flags on the floor item */
    floor_items[spawned].flags |= (inventory[slot].flags & (IFLAG_IDENTIFIED | IFLAG_BUC_KNOWN | IFLAG_CURSED | IFLAG_BLESSED));

    inventory_remove(slot);
    ui_message("Dropped.");
}

/* Forward declarations for static helpers */
static uint8_t find_equipped_in_slot(uint8_t slot_class);

/* ------------------------------------------------------------------ */
/* Use (eat / quaff / read / zap / equip toggle)                       */
/* ------------------------------------------------------------------ */

static void use_food(uint8_t slot) {
    uint8_t type_id;
    uint16_t nutrition;

    type_id   = inventory[slot].type_id;
    nutrition = (uint16_t)item_types[type_id].effect * 10;

    player.nutrition += nutrition;
    if (player.nutrition > 2000) {
        player.nutrition = 2000;
    }

    /* Update hunger state */
    if (player.nutrition > 1500) {
        player.hunger_state = HUNGER_SATIATED;
    } else if (player.nutrition > 500) {
        player.hunger_state = HUNGER_NORMAL;
    }

    inventory_remove(slot);
    ui_message("Delicious!");
}

static void use_potion(uint8_t slot) {
    uint8_t type_id;
    uint8_t effect;

    type_id = inventory[slot].type_id;
    effect  = item_types[type_id].effect;

    switch (effect) {
        case 0: /* Healing */
            player_heal(rng_roll(2, 6) + 2);
            ui_message("You feel better.");
            break;
        case 1: /* Poison */
            player_take_damage(rng_roll(1, 6));
            if (player.strength > 3) {
                player.strength--;
            }
            ui_message("You feel very sick!");
            break;
        case 2: /* Blindness */
            ui_message("Everything goes dark!");
            break;
        case 3: /* Invisibility */
            ui_message("You feel transparent.");
            break;
        case 4: /* Speed */
            ui_message("You feel quick!");
            break;
        case 5: /* Booze */
            player.nutrition += 50;
            ui_message("Ooph! That was strong!");
            break;
    }

    /* Mark this potion type as identified */
    identified_potions |= (1 << effect);

    inventory_remove(slot);
}

static void use_scroll(uint8_t slot) {
    uint8_t type_id;
    uint8_t effect;

    type_id = inventory[slot].type_id;
    effect  = item_types[type_id].effect;

    switch (effect) {
        case 0: { /* Identify */
            uint8_t sel;
            sel = ui_inventory_screen(255);
            if (sel != 255 && inventory[sel].type_id != 255) {
                inventory[sel].flags |= IFLAG_IDENTIFIED;
                ui_message(item_name(inventory[sel].type_id));
            }
            break;
        }
        case 1: /* Teleport */
            dungeon_find_random_floor(&player.x, &player.y);
            ui_message("You feel disoriented.");
            break;
        case 2: { /* Enchant weapon */
            uint8_t wpn;
            wpn = find_equipped_in_slot(3);
            if (wpn != 255) {
                inventory[wpn].flags |= IFLAG_IDENTIFIED;
                ui_message("Weapon glows blue!");
            } else {
                ui_message("You feel a tingle.");
            }
            break;
        }
        case 3: { /* Remove curse */
            uint8_t ci;
            for (ci = 0; ci < MAX_INVENTORY; ci++) {
                if (inventory[ci].type_id == 255) continue;
                inventory[ci].flags &= ~IFLAG_CURSED;
            }
            ui_message("You feel graceful.");
            break;
        }
        case 4: { /* Fire */
            int8_t dx, dy;
            uint8_t midx;
            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    midx = monster_at(player.x + dx, player.y + dy);
                    if (midx != 255) {
                        monster_take_damage(midx, rng_roll(2, 6));
                    }
                }
            }
            ui_message("Flames erupt!");
            break;
        }
        case 5: /* Blank */
            ui_message("The scroll is blank.");
            break;
    }

    /* Mark this scroll type as identified */
    identified_scrolls |= (1 << effect);

    inventory_remove(slot);
}

static void use_wand(uint8_t slot) {
    /* Wands need directional input; set a flag for the game loop */
    /* The actual zap effect is handled elsewhere */
    if (inventory[slot].quantity == 0) {
        ui_message("Nothing happens.");
        return;
    }

    ui_message("You zap the wand.");
}

void inventory_use(uint8_t slot) BANKED {
    uint8_t cat;

    if (slot >= MAX_INVENTORY) return;
    if (inventory[slot].type_id == 255) return;

    cat = item_types[inventory[slot].type_id].category;

    switch (cat) {
        case ICAT_FOOD:
            use_food(slot);
            break;
        case ICAT_POTION:
            use_potion(slot);
            break;
        case ICAT_SCROLL:
            use_scroll(slot);
            break;
        case ICAT_WAND:
            use_wand(slot);
            break;
        case ICAT_WEAPON:
        case ICAT_ARMOR:
            /* Toggle equip */
            if (inventory[slot].flags & IFLAG_EQUIPPED) {
                inventory_unequip(slot);
            } else {
                inventory_equip(slot);
            }
            break;
        case ICAT_TOOL:
            if (inventory[slot].type_id == 34) {
                /* Pickaxe: toggle equip like a weapon */
                if (inventory[slot].flags & IFLAG_EQUIPPED) {
                    inventory_unequip(slot);
                } else {
                    /* Unequip any other pickaxe first */
                    {
                        uint8_t pi;
                        for (pi = 0; pi < MAX_INVENTORY; pi++) {
                            if (inventory[pi].type_id == 34 &&
                                (inventory[pi].flags & IFLAG_EQUIPPED)) {
                                inventory[pi].flags &= ~IFLAG_EQUIPPED;
                            }
                        }
                    }
                    inventory[slot].flags |= IFLAG_EQUIPPED;
                    ui_message("You wield the pick.");
                }
            } else {
                ui_message("Can't use that.");
            }
            break;
        default:
            ui_message("Can't use that.");
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Equip / unequip                                                     */
/* ------------------------------------------------------------------ */

/*
 * Armor slot classification:
 *   6  Leather armor = body
 *   7  Chain mail    = body
 *   8  Plate mail    = body
 *   9  Shield        = shield
 *   10 Helmet        = helmet
 */

/* Returns 0=body, 1=shield, 2=helmet for armor; 3 otherwise */
static uint8_t armor_slot_type(uint8_t type_id) {
    if (type_id == 9) return 1;   /* Shield */
    if (type_id == 10) return 2;  /* Helmet */
    if (type_id >= 6 && type_id <= 8) return 0; /* Body armor */
    return 3;
}

/* Find the currently equipped item in a given armor slot class */
static uint8_t find_equipped_in_slot(uint8_t slot_class) {
    uint8_t i;
    uint8_t tid;
    for (i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].type_id == 255) continue;
        if (!(inventory[i].flags & IFLAG_EQUIPPED)) continue;

        tid = inventory[i].type_id;

        if (slot_class == 3) {
            /* Weapon slot */
            if (item_types[tid].category == ICAT_WEAPON) return i;
        } else {
            /* Armor slot */
            if (item_types[tid].category == ICAT_ARMOR &&
                armor_slot_type(tid) == slot_class) {
                return i;
            }
        }
    }
    return 255;
}

void inventory_equip(uint8_t slot) BANKED {
    uint8_t cat;
    uint8_t type_id;
    uint8_t slot_class;
    uint8_t prev;

    if (slot >= MAX_INVENTORY) return;
    if (inventory[slot].type_id == 255) return;

    type_id = inventory[slot].type_id;
    cat = item_types[type_id].category;

    if (cat == ICAT_WEAPON) {
        /* Unequip previous weapon if any */
        prev = find_equipped_in_slot(3);
        if (prev != 255) {
            /* Check if previous is cursed */
            if (inventory[prev].flags & IFLAG_CURSED) {
                inventory[prev].flags |= IFLAG_BUC_KNOWN;
                ui_message("It's cursed!");
                return;
            }
            inventory[prev].flags &= ~IFLAG_EQUIPPED;
        }
        inventory[slot].flags |= IFLAG_EQUIPPED;
        ui_message("You wield it.");
    } else if (cat == ICAT_ARMOR) {
        slot_class = armor_slot_type(type_id);
        if (slot_class > 2) return;  /* Not equippable armor */

        /* Unequip previous item in same slot */
        prev = find_equipped_in_slot(slot_class);
        if (prev != 255) {
            if (inventory[prev].flags & IFLAG_CURSED) {
                inventory[prev].flags |= IFLAG_BUC_KNOWN;
                ui_message("It's cursed!");
                return;
            }
            inventory[prev].flags &= ~IFLAG_EQUIPPED;
        }
        inventory[slot].flags |= IFLAG_EQUIPPED;
        ui_message("You put it on.");
        refresh_armor_ac();
    }
    /* Other categories cannot be equipped */
}

void inventory_unequip(uint8_t slot) BANKED {
    uint8_t tid;
    uint8_t was_armor;

    if (slot >= MAX_INVENTORY) return;
    if (inventory[slot].type_id == 255) return;
    if (!(inventory[slot].flags & IFLAG_EQUIPPED)) return;

    /* Check for curse */
    if (inventory[slot].flags & IFLAG_CURSED) {
        /* Reveal BUC status */
        inventory[slot].flags |= IFLAG_BUC_KNOWN;
        ui_message("It's cursed!");
        return;
    }

    tid = inventory[slot].type_id;
    was_armor = (item_types[tid].category == ICAT_ARMOR);

    inventory[slot].flags &= ~IFLAG_EQUIPPED;
    ui_message("You remove it.");

    if (was_armor) {
        refresh_armor_ac();
    }
}

/* ------------------------------------------------------------------ */
/* Equipment stat queries                                              */
/* ------------------------------------------------------------------ */

uint8_t inventory_get_weapon_damage(void) BANKED {
    uint8_t i;
    uint8_t tid;

    for (i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].type_id == 255) continue;
        if (!(inventory[i].flags & IFLAG_EQUIPPED)) continue;

        tid = inventory[i].type_id;
        if (item_types[tid].category == ICAT_WEAPON) {
            return item_types[tid].effect;  /* damage dice sides */
        }
    }
    return 4;  /* bare hands: 1d4 */
}

int8_t inventory_get_armor_ac(void) BANKED {
    uint8_t i;
    uint8_t tid;
    int8_t total_ac;

    total_ac = 0;

    for (i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].type_id == 255) continue;
        if (!(inventory[i].flags & IFLAG_EQUIPPED)) continue;

        tid = inventory[i].type_id;
        if (item_types[tid].category == ICAT_ARMOR) {
            total_ac += (int8_t)item_types[tid].effect;
        }
    }

    return total_ac;  /* positive = better defense */
}

uint8_t inventory_get_equipped_pickaxe(void) BANKED {
    uint8_t i;
    for (i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].type_id == 34 &&
            (inventory[i].flags & IFLAG_EQUIPPED)) {
            return i;
        }
    }
    return 255;
}
