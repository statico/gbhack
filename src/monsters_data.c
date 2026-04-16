/*
 * monsters_data.c — Const monster type table.
 * Stays in ROM bank 0 so all banked code can access monster_types[].
 */
#include "monsters.h"

#define MONSTER_TILE_BASE 64

const MonsterType monster_types[] = {
    /* 0  Newt         : */
    { MONSTER_TILE_BASE + 0,   1,  2,  8, 1, 2, SPEED_NORMAL, AI_WANDER,     0 },
    /* 1  Jackal       d */
    { MONSTER_TILE_BASE + 1,   1,  4,  7, 1, 4, SPEED_NORMAL, AI_AGGRESSIVE, 0 },
    /* 2  Bat          B */
    { MONSTER_TILE_BASE + 2,   2,  4,  8, 1, 3, SPEED_FAST,   AI_ERRATIC,    0 },
    /* 3  Kobold       k */
    { MONSTER_TILE_BASE + 3,   2,  6,  7, 1, 6, SPEED_NORMAL, AI_AGGRESSIVE, 0 },
    /* 4  Goblin       o */
    { MONSTER_TILE_BASE + 4,   2,  6,  6, 1, 6, SPEED_NORMAL, AI_AGGRESSIVE, 0 },
    /* 5  Giant rat    r */
    { MONSTER_TILE_BASE + 5,   2,  5,  7, 1, 4, SPEED_NORMAL, AI_AGGRESSIVE, 0 },
    /* 6  Snake        S */
    { MONSTER_TILE_BASE + 6,   3,  6,  6, 1, 4, SPEED_NORMAL, AI_AGGRESSIVE, MFLAG_POISON },
    /* 7  Acid blob    b */
    { MONSTER_TILE_BASE + 7,   3,  8,  8, 0, 0, SPEED_SLOW,   AI_PASSIVE,    0 },
    /* 8  Floating eye e */
    { MONSTER_TILE_BASE + 8,   3, 10,  9, 0, 0, SPEED_SLOW,   AI_PASSIVE,    MFLAG_PARALYZE },
    /* 9  Gnome        G */
    { MONSTER_TILE_BASE + 9,   4,  8,  5, 1, 6, SPEED_NORMAL, AI_WANDER,     0 },
    /* 10 Orc          O */
    { MONSTER_TILE_BASE + 10,  4, 10,  5, 1, 8, SPEED_NORMAL, AI_AGGRESSIVE, 0 },
    /* 11 Zombie       Z */
    { MONSTER_TILE_BASE + 11,  5, 12,  5, 1, 6, SPEED_SLOW,   AI_AGGRESSIVE, 0 },
    /* 12 Imp          i */
    { MONSTER_TILE_BASE + 12,  5,  8,  4, 1, 6, SPEED_NORMAL, AI_AGGRESSIVE, MFLAG_TELEPORT },
    /* 13 Nymph        n */
    { MONSTER_TILE_BASE + 13,  6,  8,  9, 0, 0, SPEED_NORMAL, AI_WANDER,     MFLAG_STEAL },
    /* 14 Mimic        m */
    { MONSTER_TILE_BASE + 14,  6, 15,  5, 1, 8, SPEED_SLOW,   AI_PASSIVE,    MFLAG_DISGUISE },
    /* 15 Giant spider s */
    { MONSTER_TILE_BASE + 15,  7, 14,  4, 1, 8, SPEED_NORMAL, AI_AGGRESSIVE, MFLAG_POISON },
    /* 16 Owlbear      Y */
    { MONSTER_TILE_BASE + 16,  8, 20,  3, 2, 6, SPEED_NORMAL, AI_AGGRESSIVE, 0 },
    /* 17 Wraith       W */
    { MONSTER_TILE_BASE + 17,  9, 16,  3, 1, 8, SPEED_NORMAL, AI_AGGRESSIVE, MFLAG_DRAIN },
    /* 18 Cockatrice   c */
    { MONSTER_TILE_BASE + 18, 10, 12,  5, 1, 4, SPEED_NORMAL, AI_PASSIVE,    MFLAG_PETRIFY },
    /* 19 Troll        T */
    { MONSTER_TILE_BASE + 19, 11, 30,  2, 2, 8, SPEED_NORMAL, AI_AGGRESSIVE, MFLAG_REGEN },
    /* 20 Vampire      V */
    { MONSTER_TILE_BASE + 20, 12, 25,  1, 1, 10, SPEED_NORMAL, AI_AGGRESSIVE, MFLAG_DRAIN },
    /* 21 Dragon       D */
    { MONSTER_TILE_BASE + 21, 14, 40, -1, 3, 8, SPEED_NORMAL, AI_AGGRESSIVE, 0 },
    /* 22 Shopkeeper   @ */
    { MONSTER_TILE_BASE + 22, 15, 50,  0, 4, 6, SPEED_NORMAL, AI_PASSIVE,    0 },
    /* 23 Ghost        X */
    { MONSTER_TILE_BASE + 23, 10, 20, -2, 1, 6, SPEED_NORMAL, AI_AGGRESSIVE, 0 },
    /* 24 Wizard       & */
    { MONSTER_TILE_BASE + 24, 15, 60, -3, 3, 10, SPEED_NORMAL, AI_AGGRESSIVE, 0 },
};

uint8_t num_monster_types = 25;

static const char *const monster_names[] = {
    "Newt", "Jackal", "Bat", "Kobold", "Goblin",
    "Giant rat", "Snake", "Acid blob", "Floating eye", "Gnome",
    "Orc", "Zombie", "Imp", "Nymph", "Mimic",
    "Giant spider", "Owlbear", "Wraith", "Cockatrice", "Troll",
    "Vampire", "Dragon", "Shopkeeper", "Ghost", "Wizard"
};

const char *monster_name(uint8_t type_id) {
    if (type_id >= num_monster_types) return "something";
    return monster_names[type_id];
}
