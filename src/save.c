#pragma bank 6

#include "save.h"
#include "player.h"
#include "dungeon.h"
#include "monsters.h"
#include "items.h"
#include "inventory.h"
#include "rng.h"

/*
 * SRAM layout:
 *   Bank 0: save_marker (0xA000), save_checksum (0xA001),
 *           Player struct, inventory array, identification tables
 *   Bank 1: dungeon_map, monsters array, floor_items
 *   Bank 2: bones data + high score
 */

#define SAVE_MARKER_VALID 0x42

/* SRAM base addresses for our data (after marker + checksum) */
#define SRAM_BASE          0xA000
#define SRAM_PLAYER_ADDR   0xA002
#define SRAM_INVENTORY_ADDR (SRAM_PLAYER_ADDR + sizeof(Player))
#define SRAM_INV_COUNT_ADDR (SRAM_INVENTORY_ADDR + sizeof(Item) * MAX_INVENTORY)
#define SRAM_POTION_APP_ADDR (SRAM_INV_COUNT_ADDR + 1)
#define SRAM_SCROLL_APP_ADDR (SRAM_POTION_APP_ADDR + 6)
#define SRAM_ID_POTIONS_ADDR (SRAM_SCROLL_APP_ADDR + 6)
#define SRAM_ID_SCROLLS_ADDR (SRAM_ID_POTIONS_ADDR + 1)

/* Bank 1 addresses */
#define SRAM_MAP_ADDR      0xA000
#define SRAM_ROOMS_ADDR    (SRAM_MAP_ADDR + MAP_WIDTH * MAP_HEIGHT)
#define SRAM_NUM_ROOMS_ADDR (SRAM_ROOMS_ADDR + sizeof(Room) * MAX_ROOMS)
#define SRAM_STAIRS_ADDR   (SRAM_NUM_ROOMS_ADDR + 1)
#define SRAM_MONSTERS_ADDR (SRAM_STAIRS_ADDR + 4)
#define SRAM_FLOOR_ITEMS_ADDR (SRAM_MONSTERS_ADDR + sizeof(Monster) * MAX_MONSTERS)

/* Bank 2 addresses (bones + hiscore) */
#define SRAM_BONES_MARKER  0xA000
#define SRAM_BONES_X       0xA001
#define SRAM_BONES_Y       0xA002
#define SRAM_BONES_LEVEL   0xA003
#define SRAM_BONES_ITEMS   0xA004  /* 3 Item structs */
#define SRAM_HISCORE_ADDR  (SRAM_BONES_ITEMS + sizeof(Item) * 3)
#define SRAM_HISCORE_MARKER (SRAM_HISCORE_ADDR + 2)

#define BONES_MARKER_VALID 0xB0
#define HISCORE_MARKER_VALID 0xA5

/* ------------------------------------------------------------------ */
/* Helper: copy bytes to/from SRAM                                     */
/* ------------------------------------------------------------------ */

static void sram_write(uint16_t addr, const void *src, uint16_t len) {
    const uint8_t *s = (const uint8_t *)src;
    volatile uint8_t *d = (volatile uint8_t *)addr;
    uint16_t i;
    for (i = 0; i < len; i++) {
        d[i] = s[i];
    }
}

static void sram_read(uint16_t addr, void *dst, uint16_t len) {
    volatile uint8_t *s = (volatile uint8_t *)addr;
    uint8_t *d = (uint8_t *)dst;
    uint16_t i;
    for (i = 0; i < len; i++) {
        d[i] = s[i];
    }
}

/* ------------------------------------------------------------------ */
/* Checksum                                                            */
/* ------------------------------------------------------------------ */

static uint8_t compute_checksum(void) {
    uint8_t sum = 0;
    uint8_t *p;
    uint16_t i;

    /* Checksum over player struct */
    p = (uint8_t *)&player;
    for (i = 0; i < sizeof(Player); i++) {
        sum += p[i];
    }

    /* Checksum over inventory */
    p = (uint8_t *)inventory;
    for (i = 0; i < sizeof(Item) * MAX_INVENTORY; i++) {
        sum += p[i];
    }

    sum += inventory_count;

    /* Checksum over identification tables */
    for (i = 0; i < 6; i++) {
        sum += potion_appearances[i];
        sum += scroll_appearances[i];
    }
    sum += identified_potions;
    sum += identified_scrolls;

    return sum;
}

/* ------------------------------------------------------------------ */
/* Save                                                                */
/* ------------------------------------------------------------------ */

void save_game(void) BANKED {
    uint8_t checksum;

    /* Compute checksum before touching SRAM */
    checksum = compute_checksum();

    ENABLE_RAM;

    /* Bank 0: player state */
    SWITCH_RAM(0);
    sram_write(SRAM_PLAYER_ADDR, &player, sizeof(Player));
    sram_write(SRAM_INVENTORY_ADDR, inventory, sizeof(Item) * MAX_INVENTORY);
    sram_write(SRAM_INV_COUNT_ADDR, &inventory_count, 1);
    sram_write(SRAM_POTION_APP_ADDR, potion_appearances, 6);
    sram_write(SRAM_SCROLL_APP_ADDR, scroll_appearances, 6);
    sram_write(SRAM_ID_POTIONS_ADDR, &identified_potions, 1);
    sram_write(SRAM_ID_SCROLLS_ADDR, &identified_scrolls, 1);

    /* Write marker + checksum last */
    *((volatile uint8_t *)SRAM_BASE) = SAVE_MARKER_VALID;
    *((volatile uint8_t *)(SRAM_BASE + 1)) = checksum;

    /* Bank 1: dungeon state */
    SWITCH_RAM(1);
    sram_write(SRAM_MAP_ADDR, dungeon_map, MAP_WIDTH * MAP_HEIGHT);
    sram_write(SRAM_ROOMS_ADDR, dungeon_rooms, sizeof(Room) * MAX_ROOMS);
    sram_write(SRAM_NUM_ROOMS_ADDR, &dungeon_num_rooms, 1);
    /* Pack stairs coordinates */
    {
        uint8_t stairs_buf[4];
        stairs_buf[0] = stairs_up_x;
        stairs_buf[1] = stairs_up_y;
        stairs_buf[2] = stairs_down_x;
        stairs_buf[3] = stairs_down_y;
        sram_write(SRAM_STAIRS_ADDR, stairs_buf, 4);
    }
    sram_write(SRAM_MONSTERS_ADDR, monsters, sizeof(Monster) * MAX_MONSTERS);
    sram_write(SRAM_FLOOR_ITEMS_ADDR, floor_items, sizeof(Item) * MAX_ITEMS);

    DISABLE_RAM;
}

/* ------------------------------------------------------------------ */
/* Load                                                                */
/* ------------------------------------------------------------------ */

uint8_t save_load(void) BANKED {
    uint8_t marker;
    uint8_t stored_checksum;
    uint8_t checksum;

    ENABLE_RAM;
    SWITCH_RAM(0);

    marker = *((volatile uint8_t *)SRAM_BASE);
    stored_checksum = *((volatile uint8_t *)(SRAM_BASE + 1));

    if (marker != SAVE_MARKER_VALID) {
        DISABLE_RAM;
        return 0;
    }

    /* Read player state from bank 0 */
    sram_read(SRAM_PLAYER_ADDR, &player, sizeof(Player));
    sram_read(SRAM_INVENTORY_ADDR, inventory, sizeof(Item) * MAX_INVENTORY);
    sram_read(SRAM_INV_COUNT_ADDR, &inventory_count, 1);
    sram_read(SRAM_POTION_APP_ADDR, potion_appearances, 6);
    sram_read(SRAM_SCROLL_APP_ADDR, scroll_appearances, 6);
    sram_read(SRAM_ID_POTIONS_ADDR, &identified_potions, 1);
    sram_read(SRAM_ID_SCROLLS_ADDR, &identified_scrolls, 1);

    /* Read dungeon state from bank 1 */
    SWITCH_RAM(1);
    sram_read(SRAM_MAP_ADDR, dungeon_map, MAP_WIDTH * MAP_HEIGHT);
    sram_read(SRAM_ROOMS_ADDR, dungeon_rooms, sizeof(Room) * MAX_ROOMS);
    sram_read(SRAM_NUM_ROOMS_ADDR, &dungeon_num_rooms, 1);
    {
        uint8_t stairs_buf[4];
        sram_read(SRAM_STAIRS_ADDR, stairs_buf, 4);
        stairs_up_x   = stairs_buf[0];
        stairs_up_y   = stairs_buf[1];
        stairs_down_x = stairs_buf[2];
        stairs_down_y = stairs_buf[3];
    }
    sram_read(SRAM_MONSTERS_ADDR, monsters, sizeof(Monster) * MAX_MONSTERS);
    sram_read(SRAM_FLOOR_ITEMS_ADDR, floor_items, sizeof(Item) * MAX_ITEMS);

    DISABLE_RAM;

    /* Verify checksum against loaded data */
    checksum = compute_checksum();
    if (checksum != stored_checksum) {
        /* Corrupted save -- treat as no save */
        return 0;
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* Delete                                                              */
/* ------------------------------------------------------------------ */

void save_delete(void) BANKED {
    ENABLE_RAM;
    SWITCH_RAM(0);
    *((volatile uint8_t *)SRAM_BASE) = 0x00;      /* invalidate marker */
    *((volatile uint8_t *)(SRAM_BASE + 1)) = 0x00; /* clear checksum */
    DISABLE_RAM;
}

/* ------------------------------------------------------------------ */
/* Bones                                                               */
/* ------------------------------------------------------------------ */

void save_write_bones(void) BANKED {
    uint8_t i;
    uint8_t count;
    Item bones_items[3];

    /* Initialize bones items as empty */
    for (i = 0; i < 3; i++) {
        bones_items[i].type_id  = 255;
        bones_items[i].x        = 255;
        bones_items[i].y        = 255;
        bones_items[i].quantity  = 0;
        bones_items[i].flags    = 0;
    }

    /* Pick up to 3 random items from inventory */
    count = 0;
    for (i = 0; i < MAX_INVENTORY && count < 3; i++) {
        if (inventory[i].type_id != 255) {
            /* 50% chance to include each item, ensures variety */
            if (rng_range(0, 1) == 0 || count == 0) {
                bones_items[count] = inventory[i];
                bones_items[count].x = player.x;
                bones_items[count].y = player.y;
                bones_items[count].flags = IFLAG_ON_GROUND;
                count++;
            }
        }
    }

    ENABLE_RAM;
    SWITCH_RAM(2);
    *((volatile uint8_t *)SRAM_BONES_MARKER) = BONES_MARKER_VALID;
    *((volatile uint8_t *)SRAM_BONES_X) = player.x;
    *((volatile uint8_t *)SRAM_BONES_Y) = player.y;
    *((volatile uint8_t *)SRAM_BONES_LEVEL) = player.dungeon_level;
    sram_write(SRAM_BONES_ITEMS, bones_items, sizeof(Item) * 3);
    DISABLE_RAM;
}

/* ------------------------------------------------------------------ */
/* High score                                                          */
/* ------------------------------------------------------------------ */

void save_write_hiscore(uint16_t score) BANKED {
    uint16_t current;

    current = save_get_hiscore();
    if (score <= current) {
        return;
    }

    ENABLE_RAM;
    SWITCH_RAM(2);
    sram_write(SRAM_HISCORE_ADDR, &score, 2);
    *((volatile uint8_t *)SRAM_HISCORE_MARKER) = HISCORE_MARKER_VALID;
    DISABLE_RAM;
}

uint16_t save_get_hiscore(void) BANKED {
    uint8_t marker;
    uint16_t score;

    ENABLE_RAM;
    SWITCH_RAM(2);
    marker = *((volatile uint8_t *)SRAM_HISCORE_MARKER);
    if (marker != HISCORE_MARKER_VALID) {
        DISABLE_RAM;
        return 0;
    }
    sram_read(SRAM_HISCORE_ADDR, &score, 2);
    DISABLE_RAM;
    return score;
}
