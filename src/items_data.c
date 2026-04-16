/*
 * items_data.c — Const item tables and name lookup functions.
 * Stays in ROM bank 0 so all banked code can access item_types[].
 */
#include "items.h"

#define ITEM_TILE_BASE 128

const ItemType item_types[] = {
    /* 0  Dagger       */ { ITEM_TILE_BASE +  0, ICAT_WEAPON, 10,  4,  4 },
    /* 1  Short sword  */ { ITEM_TILE_BASE +  1, ICAT_WEAPON, 30, 10,  6 },
    /* 2  Long sword   */ { ITEM_TILE_BASE +  2, ICAT_WEAPON, 40, 15,  8 },
    /* 3  Mace         */ { ITEM_TILE_BASE +  3, ICAT_WEAPON, 30, 10,  7 },
    /* 4  Bow          */ { ITEM_TILE_BASE +  4, ICAT_WEAPON, 30, 12,  2 },
    /* 5  Arrow        */ { ITEM_TILE_BASE +  5, ICAT_WEAPON,  1,  1,  6 },

    /* 6  Leather armor */ { ITEM_TILE_BASE + 6, ICAT_ARMOR, 15, 10,  2 },
    /* 7  Chain mail    */ { ITEM_TILE_BASE + 7, ICAT_ARMOR, 40, 30,  4 },
    /* 8  Plate mail    */ { ITEM_TILE_BASE + 8, ICAT_ARMOR, 60, 50,  6 },
    /* 9  Shield        */ { ITEM_TILE_BASE + 9, ICAT_ARMOR, 10,  7,  1 },
    /* 10 Helmet        */ { ITEM_TILE_BASE + 10, ICAT_ARMOR,  5,  5,  1 },

    /* 11 Healing       */ { ITEM_TILE_BASE + 11, ICAT_POTION, 5, 20, 0 },
    /* 12 Poison        */ { ITEM_TILE_BASE + 12, ICAT_POTION, 5, 20, 1 },
    /* 13 Blindness     */ { ITEM_TILE_BASE + 13, ICAT_POTION, 5, 20, 2 },
    /* 14 Invisibility  */ { ITEM_TILE_BASE + 14, ICAT_POTION, 5, 50, 3 },
    /* 15 Speed         */ { ITEM_TILE_BASE + 15, ICAT_POTION, 5, 40, 4 },
    /* 16 Booze         */ { ITEM_TILE_BASE + 16, ICAT_POTION, 5, 10, 5 },

    /* 17 Identify      */ { ITEM_TILE_BASE + 17, ICAT_SCROLL, 3, 20, 0 },
    /* 18 Teleport      */ { ITEM_TILE_BASE + 18, ICAT_SCROLL, 3, 30, 1 },
    /* 19 Enchant weapon*/ { ITEM_TILE_BASE + 19, ICAT_SCROLL, 3, 40, 2 },
    /* 20 Remove curse  */ { ITEM_TILE_BASE + 20, ICAT_SCROLL, 3, 30, 3 },
    /* 21 Fire          */ { ITEM_TILE_BASE + 21, ICAT_SCROLL, 3, 50, 4 },
    /* 22 Blank         */ { ITEM_TILE_BASE + 22, ICAT_SCROLL, 3,  5, 5 },

    /* 23 Wand of Fire  */ { ITEM_TILE_BASE + 23, ICAT_WAND, 7, 50, 0 },
    /* 24 Wand of Cold  */ { ITEM_TILE_BASE + 24, ICAT_WAND, 7, 50, 1 },
    /* 25 Wand of Sleep */ { ITEM_TILE_BASE + 25, ICAT_WAND, 7, 50, 2 },
    /* 26 Wand of Digging*/ { ITEM_TILE_BASE + 26, ICAT_WAND, 7, 50, 3 },
    /* 27 Wand of Death */ { ITEM_TILE_BASE + 27, ICAT_WAND, 7, 100, 4 },

    /* 28 Ration        */ { ITEM_TILE_BASE + 28, ICAT_FOOD, 20, 10, 80 },
    /* 29 Apple         */ { ITEM_TILE_BASE + 29, ICAT_FOOD,  5,  5, 20 },
    /* 30 Corpse        */ { ITEM_TILE_BASE + 30, ICAT_FOOD, 30,  0, 10 },
    /* 31 Tin           */ { ITEM_TILE_BASE + 31, ICAT_FOOD, 10,  8, 30 },

    /* 32 Gold          */ { ITEM_TILE_BASE + 32, ICAT_GOLD,   1, 0, 0 },
    /* 33 Key           */ { ITEM_TILE_BASE + 33, ICAT_TOOL,   3, 5, 0 },
    /* 34 Pickaxe       */ { ITEM_TILE_BASE + 34, ICAT_TOOL,  30,20, 1 },
    /* 35 Amulet of Yendor*/ { ITEM_TILE_BASE + 35, ICAT_AMULET, 3, 0, 0 },
    /* 36 Sack          */ { ITEM_TILE_BASE + 36, ICAT_TOOL,   5,10, 2 },
    /* 37 Lamp          */ { ITEM_TILE_BASE + 37, ICAT_TOOL,  10,15, 3 },
    /* 38 Stethoscope   */ { ITEM_TILE_BASE + 38, ICAT_TOOL,   4,25, 4 },
    /* 39 Whistle       */ { ITEM_TILE_BASE + 39, ICAT_TOOL,   3, 5, 5 },
};

uint8_t num_item_types = 40;

static const char *const item_names[] = {
    "Dagger", "Short sword", "Long sword", "Mace", "Bow", "Arrow",
    "Leather armor", "Chain mail", "Plate mail", "Shield", "Helmet",
    "Healing", "Poison", "Blindness", "Invisibility", "Speed", "Booze",
    "Identify", "Teleport", "Enchant weapon", "Remove curse", "Fire", "Blank",
    "Wand of Fire", "Wand of Cold", "Wand of Sleep", "Wand of Digging", "Wand of Death",
    "Ration", "Apple", "Corpse", "Tin",
    "Gold", "Key", "Pickaxe", "Amulet of Yendor", "Sack", "Lamp", "Stethoscope", "Whistle"
};

static const char *const potion_appearance_names[] = {
    "bubbly", "smoky", "milky", "fizzy", "dark", "glowing"
};

static const char *const scroll_appearance_names[] = {
    "ZELGO MER", "TEMOV", "GARVEN DEH", "ELAM EBOW", "NR 9", "FOOBIE BLETCH"
};

const char *item_name(uint8_t type_id) {
    if (type_id >= num_item_types) return "???";
    return item_names[type_id];
}

const char *item_appearance_name(uint8_t type_id) {
    static char buf[21];
    uint8_t cat;
    uint8_t effect;
    uint8_t appearance_idx;
    const char *src;
    uint8_t i, j;

    if (type_id >= num_item_types) return "???";

    cat = item_types[type_id].category;
    effect = item_types[type_id].effect;

    if (cat == ICAT_POTION) {
        if (identified_potions & (1 << effect)) {
            return item_names[type_id];
        }
        src = 0;
        for (appearance_idx = 0; appearance_idx < 6; appearance_idx++) {
            if (potion_appearances[appearance_idx] == effect) {
                src = potion_appearance_names[appearance_idx];
                break;
            }
        }
        if (!src) return "strange potion";
        /* Build "<color> potion" */
        i = 0;
        for (j = 0; src[j] && i < 13; j++) buf[i++] = src[j];
        buf[i++] = ' ';
        buf[i++] = 'p'; buf[i++] = 'o'; buf[i++] = 't';
        buf[i++] = 'i'; buf[i++] = 'o'; buf[i++] = 'n';
        buf[i] = '\0';
        return buf;
    }

    if (cat == ICAT_SCROLL) {
        if (identified_scrolls & (1 << effect)) {
            return item_names[type_id];
        }
        src = 0;
        for (appearance_idx = 0; appearance_idx < 6; appearance_idx++) {
            if (scroll_appearances[appearance_idx] == effect) {
                src = scroll_appearance_names[appearance_idx];
                break;
            }
        }
        if (!src) return "strange scroll";
        /* Build "Scroll: <name>" */
        buf[0] = 'S'; buf[1] = 'c'; buf[2] = 'r'; buf[3] = 'o';
        buf[4] = 'l'; buf[5] = 'l'; buf[6] = ':'; buf[7] = ' ';
        i = 8;
        for (j = 0; src[j] && i < 20; j++) buf[i++] = src[j];
        buf[i] = '\0';
        return buf;
    }

    return item_names[type_id];
}
