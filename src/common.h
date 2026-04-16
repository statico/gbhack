#ifndef COMMON_H
#define COMMON_H

#include <gb/gb.h>
#include <stdint.h>

// Map dimensions
#define MAP_WIDTH  40
#define MAP_HEIGHT 30
#define VIEWPORT_W 20
#define VIEWPORT_H 14
#define STATUS_ROWS 2
#define MSG_ROWS    2

// Max entities
#define MAX_MONSTERS 30
#define MAX_ITEMS    50
#define MAX_INVENTORY 20
#define MAX_DUNGEON_LEVELS 15
#define MAX_ROOMS 8

// Terrain types (bits 0-3 of cell)
#define TERRAIN_MASK   0x0F
#define TERRAIN_WALL   0x00
#define TERRAIN_FLOOR  0x01
#define TERRAIN_CORRIDOR 0x02
#define TERRAIN_DOOR_CLOSED 0x03
#define TERRAIN_DOOR_OPEN   0x04
#define TERRAIN_STAIRS_UP   0x05
#define TERRAIN_STAIRS_DOWN 0x06
#define TERRAIN_ALTAR  0x07
#define TERRAIN_FOUNTAIN 0x08
#define TERRAIN_TRAP   0x09
#define TERRAIN_SHOP_FLOOR 0x0A

// Cell flags (bits 4-7)
#define CELL_LIT      0x10
#define CELL_SEEN     0x20
#define CELL_HAS_ITEM 0x40
#define CELL_HAS_MONSTER 0x80

// Palette IDs
#define PAL_TERRAIN  0
#define PAL_PLAYER   1
#define PAL_HOSTILE  2
#define PAL_NEUTRAL  3
#define PAL_EQUIPMENT 4
#define PAL_CONSUMABLE 5
#define PAL_SPECIAL  6
#define PAL_UI       7

// Game states
#define STATE_TITLE     0
#define STATE_GAMEPLAY  1
#define STATE_INVENTORY 2
#define STATE_MENU      3
#define STATE_GAMEOVER  4
#define STATE_WIN       5
#define STATE_CHARSHEET 6
#define STATE_MSGLOG    7

// Hunger states
#define HUNGER_SATIATED  0
#define HUNGER_NORMAL    1
#define HUNGER_HUNGRY    2
#define HUNGER_WEAK      3
#define HUNGER_FAINTING  4
#define HUNGER_STARVED   5

// BUC status
#define BUC_UNKNOWN  0
#define BUC_CURSED   1
#define BUC_UNCURSED 2
#define BUC_BLESSED  3

// Direction
#define DIR_NONE  0
#define DIR_N     1
#define DIR_S     2
#define DIR_E     3
#define DIR_W     4
#define DIR_NE    5
#define DIR_NW    6
#define DIR_SE    7
#define DIR_SW    8

// Speed
#define SPEED_SLOW   0
#define SPEED_NORMAL 1
#define SPEED_FAST   2

// Monster behavior
#define AI_AGGRESSIVE 0
#define AI_PASSIVE    1
#define AI_ERRATIC    2
#define AI_WANDER     3
#define AI_FLEEING    4

// Room struct
typedef struct {
    uint8_t x, y;      // top-left corner
    uint8_t w, h;       // width, height
    uint8_t type;       // 0=normal, 1=shop, 2=altar room
    uint8_t lit;        // 1 if lit
} Room;

// Monster type definition (ROM, const)
typedef struct {
    uint8_t symbol;     // tile index
    uint8_t level;      // difficulty level
    uint8_t max_hp;
    int8_t  ac;
    uint8_t damage_dice;  // number of dice
    uint8_t damage_sides; // sides per die
    uint8_t speed;      // SPEED_*
    uint8_t behavior;   // AI_*
    uint8_t flags;      // special abilities bitfield
} MonsterType;

// Monster flags
#define MFLAG_POISON    0x01
#define MFLAG_PARALYZE  0x02
#define MFLAG_TELEPORT  0x04
#define MFLAG_STEAL     0x08
#define MFLAG_DISGUISE  0x10
#define MFLAG_DRAIN     0x20
#define MFLAG_REGEN     0x40
#define MFLAG_PETRIFY   0x80

// Runtime monster instance
typedef struct {
    uint8_t type_id;
    uint8_t x, y;
    uint8_t hp;
    uint8_t status;     // bitfield: asleep, confused, paralyzed, fleeing
    uint8_t target_x, target_y;
    uint8_t timer;
    uint8_t item1;      // item type carried (for drops)
    uint8_t item2;
    uint8_t padding[5];
    uint8_t active;     // 0 = slot is free
} Monster;

// Monster status bits
#define MSTAT_ASLEEP    0x01
#define MSTAT_CONFUSED  0x02
#define MSTAT_PARALYZED 0x04
#define MSTAT_FLEEING   0x08
#define MSTAT_PEACEFUL  0x10

// Item type definition (ROM, const)
typedef struct {
    uint8_t symbol;     // tile index
    uint8_t category;   // ICAT_*
    uint8_t weight;
    uint8_t base_price;
    uint8_t effect;     // category-specific effect ID
} ItemType;

// Item categories
#define ICAT_WEAPON  0
#define ICAT_ARMOR   1
#define ICAT_POTION  2
#define ICAT_SCROLL  3
#define ICAT_WAND    4
#define ICAT_FOOD    5
#define ICAT_GOLD    6
#define ICAT_TOOL    7
#define ICAT_AMULET  8

// Runtime item instance (floor or inventory)
typedef struct {
    uint8_t type_id;
    uint8_t x, y;       // position (255,255 if in inventory)
    uint8_t quantity;
    uint8_t flags;      // bits: identified, buc_known, equipped, on_ground
} Item;

// Item flags
#define IFLAG_IDENTIFIED 0x01
#define IFLAG_BUC_KNOWN  0x02
#define IFLAG_EQUIPPED   0x04
#define IFLAG_ON_GROUND  0x08
#define IFLAG_CURSED     0x10
#define IFLAG_BLESSED    0x20

// Player struct
typedef struct {
    uint8_t x, y;
    uint8_t hp, max_hp;
    int8_t  ac;
    uint8_t strength;
    uint8_t level;
    uint16_t xp;
    uint16_t gold;
    uint16_t nutrition;
    uint8_t  dungeon_level;
    uint8_t  hunger_state;
    uint16_t turns;
    uint16_t prayer_cooldown;
    uint8_t  pet_type;    // 0=none, 1=cat, 2=dog
    uint8_t  shopkeeper_hostile; // global flag
} Player;

// Helper macros
#define CELL_TERRAIN(c)   ((c) & TERRAIN_MASK)
#define CELL_IS_PASSABLE(c) (CELL_TERRAIN(c) != TERRAIN_WALL && CELL_TERRAIN(c) != TERRAIN_DOOR_CLOSED)
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define ABS(a)   ((a) < 0 ? -(a) : (a))

#endif
