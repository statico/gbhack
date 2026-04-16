#pragma bank 4

#include "pet.h"
#include "monsters.h"
#include "player.h"
#include "dungeon.h"
#include "items.h"
#include "rng.h"
#include "ui.h"

uint8_t pet_index = 255;
uint8_t pet_away_turns = 0;

/* Pet type constants matching player.pet_type */
#define PET_NONE 0
#define PET_CAT  1
#define PET_DOG  2

/* Pet stats */
#define CAT_HP     8
#define CAT_AC     7
#define CAT_DICE   1
#define CAT_SIDES  4

#define DOG_HP    10
#define DOG_AC     6
#define DOG_DICE   1
#define DOG_SIDES  6

/* Pet AI thresholds */
#define PET_FOLLOW_CLOSE  3
#define PET_FOLLOW_FAR    5
#define PET_FERAL_DIST   15
#define PET_FERAL_TURNS  50

/* Corpse item type ID */
#define ITEM_CORPSE 30

/* Pet attack bonus */
#define PET_ATTACK_BONUS 3

/* Pet heal from eating */
#define PET_EAT_HEAL 5

/* Forward declarations */
static void pet_move_toward_player(void);
static void pet_move_wander_biased(void);
static void pet_move_wander(void);
static uint8_t pet_try_move(int8_t dx, int8_t dy);
static void pet_try_attack(void);
static void pet_try_eat(void);
static uint8_t pet_tile_has_cursed_item(uint8_t x, uint8_t y);

void pet_init(uint8_t type) BANKED {
    uint8_t i;
    uint8_t slot;
    uint8_t px, py;
    uint8_t sx, sy;
    int8_t dx, dy;
    uint8_t cell;
    uint8_t found;

    if (type == PET_NONE) return;

    /* Find a passable tile adjacent to the player */
    px = player.x;
    py = player.y;
    found = 0;

    for (i = 0; i < 8; i++) {
        switch (i) {
        case 0: dx =  1; dy =  0; break;
        case 1: dx = -1; dy =  0; break;
        case 2: dx =  0; dy =  1; break;
        case 3: dx =  0; dy = -1; break;
        case 4: dx =  1; dy =  1; break;
        case 5: dx = -1; dy =  1; break;
        case 6: dx =  1; dy = -1; break;
        case 7: dx = -1; dy = -1; break;
        default: dx = 0; dy = 0; break;
        }

        sx = px + dx;
        sy = py + dy;

        if (sx >= MAP_WIDTH || sy >= MAP_HEIGHT) continue;

        if (!dungeon_is_passable(sx, sy)) continue;

        cell = dungeon_get_cell(sx, sy);
        if (cell & CELL_HAS_MONSTER) continue;

        found = 1;
        break;
    }

    if (!found) {
        /* Fallback: spawn on a random floor tile */
        dungeon_find_random_floor(&sx, &sy);
    }

    /*
     * Allocate a monster slot manually. We reuse the first monster type
     * (type_id 0, Newt) as a placeholder since we override stats below.
     * The type_id is stored but the actual stats are set directly.
     */
    slot = 255;
    for (i = 0; i < MAX_MONSTERS; i++) {
        if (!monsters[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == 255) {
        ui_message("No room for pet!");
        return;
    }

    /* Set up the monster slot */
    monsters[slot].active = 1;
    monsters[slot].type_id = 0; /* placeholder type */
    monsters[slot].x = sx;
    monsters[slot].y = sy;
    monsters[slot].status = MSTAT_PEACEFUL;
    monsters[slot].timer = 0;
    monsters[slot].target_x = 0;
    monsters[slot].target_y = 0;
    monsters[slot].item1 = 0;
    monsters[slot].item2 = 0;

    if (type == PET_CAT) {
        monsters[slot].hp = CAT_HP;
    } else {
        monsters[slot].hp = DOG_HP;
    }

    /* Mark cell as occupied */
    cell = dungeon_get_cell(sx, sy);
    dungeon_set_cell(sx, sy, cell | CELL_HAS_MONSTER);

    pet_index = slot;
    pet_away_turns = 0;
    player.pet_type = type;

    if (type == PET_CAT) {
        ui_message("A kitten follows!");
    } else {
        ui_message("A puppy follows!");
    }
}

void pet_update(void) BANKED {
    uint8_t dist;
    int8_t ddx, ddy;

    /* No pet */
    if (pet_index == 255) return;

    /* Pet died */
    if (!monsters[pet_index].active) {
        ui_message("Your pet has died!");
        pet_index = 255;
        return;
    }

    /* Calculate Manhattan distance to player */
    ddx = (int8_t)monsters[pet_index].x - (int8_t)player.x;
    ddy = (int8_t)monsters[pet_index].y - (int8_t)player.y;
    dist = ABS(ddx) + ABS(ddy);

    /* Check feral distance */
    if (dist > PET_FERAL_DIST) {
        pet_away_turns++;
        if (pet_away_turns > PET_FERAL_TURNS) {
            pet_check_feral();
            return;
        }
    } else {
        pet_away_turns = 0;
    }

    /* Combat: attack adjacent hostile monsters */
    pet_try_attack();

    /* Movement AI based on distance to player */
    if (dist > PET_FOLLOW_FAR) {
        /* Too far: move toward player */
        pet_move_toward_player();
    } else if (dist >= PET_FOLLOW_CLOSE) {
        /* Medium range: wander with bias toward player */
        pet_move_wander_biased();
    } else {
        /* Close: wander randomly */
        pet_move_wander();
    }

    /* Eating: check for corpse on current tile */
    pet_try_eat();
}

uint8_t pet_is_alive(void) BANKED {
    if (pet_index == 255) return 0;
    if (!monsters[pet_index].active) return 0;
    return 1;
}

void pet_check_feral(void) BANKED {
    if (pet_index == 255) return;

    /* Remove peaceful status */
    monsters[pet_index].status &= ~MSTAT_PEACEFUL;

    pet_index = 255;
    pet_away_turns = 0;

    ui_message("Your pet has gone");
    ui_message("feral!");
}

/* ---- Internal helpers ---- */

static uint8_t pet_tile_has_cursed_item(uint8_t x, uint8_t y) {
    uint8_t item_idx;

    item_idx = item_at(x, y);
    if (item_idx == 255) return 0;

    if (floor_items[item_idx].flags & IFLAG_CURSED) {
        return 1;
    }

    return 0;
}

static uint8_t pet_try_move(int8_t dx, int8_t dy) {
    uint8_t nx, ny;
    uint8_t cell;
    uint8_t old_cell;

    nx = monsters[pet_index].x + dx;
    ny = monsters[pet_index].y + dy;

    /* Bounds check */
    if (nx >= MAP_WIDTH || ny >= MAP_HEIGHT) return 0;

    /* Don't walk onto player */
    if (nx == player.x && ny == player.y) return 0;

    /* Must be passable */
    if (!dungeon_is_passable(nx, ny)) return 0;

    /* Don't step on another monster */
    cell = dungeon_get_cell(nx, ny);
    if (cell & CELL_HAS_MONSTER) return 0;

    /* Cursed item avoidance */
    if (pet_tile_has_cursed_item(nx, ny)) return 0;

    /* Clear old cell flag */
    old_cell = dungeon_get_cell(monsters[pet_index].x, monsters[pet_index].y);
    dungeon_set_cell(monsters[pet_index].x, monsters[pet_index].y,
                     old_cell & ~CELL_HAS_MONSTER);

    /* Move */
    monsters[pet_index].x = nx;
    monsters[pet_index].y = ny;

    /* Set new cell flag */
    dungeon_set_cell(nx, ny, cell | CELL_HAS_MONSTER);

    return 1;
}

static void pet_move_toward_player(void) {
    int8_t dx, dy;

    dx = 0;
    dy = 0;

    if (player.x > monsters[pet_index].x) dx = 1;
    else if (player.x < monsters[pet_index].x) dx = -1;

    if (player.y > monsters[pet_index].y) dy = 1;
    else if (player.y < monsters[pet_index].y) dy = -1;

    /* Try diagonal first */
    if (dx && dy) {
        if (pet_try_move(dx, dy)) return;
    }

    /* Try cardinal directions */
    if (dx) {
        if (pet_try_move(dx, 0)) return;
    }
    if (dy) {
        if (pet_try_move(0, dy)) return;
    }
}

static void pet_move_wander_biased(void) {
    uint8_t roll;
    int8_t dx, dy;

    /* 50% chance to move toward player, 50% random */
    roll = rng_range(0, 1);
    if (roll == 0) {
        pet_move_toward_player();
    } else {
        /* Random direction */
        roll = rng_range(0, 7);
        switch (roll) {
        case 0: dx =  0; dy = -1; break;
        case 1: dx =  0; dy =  1; break;
        case 2: dx =  1; dy =  0; break;
        case 3: dx = -1; dy =  0; break;
        case 4: dx =  1; dy = -1; break;
        case 5: dx = -1; dy = -1; break;
        case 6: dx =  1; dy =  1; break;
        case 7: dx = -1; dy =  1; break;
        default: dx = 0; dy = 0; break;
        }
        pet_try_move(dx, dy);
    }
}

static void pet_move_wander(void) {
    uint8_t dir;
    int8_t dx, dy;

    dir = rng_range(0, 7);
    switch (dir) {
    case 0: dx =  0; dy = -1; break;
    case 1: dx =  0; dy =  1; break;
    case 2: dx =  1; dy =  0; break;
    case 3: dx = -1; dy =  0; break;
    case 4: dx =  1; dy = -1; break;
    case 5: dx = -1; dy = -1; break;
    case 6: dx =  1; dy =  1; break;
    case 7: dx = -1; dy =  1; break;
    default: dx = 0; dy = 0; break;
    }

    pet_try_move(dx, dy);
}

static void pet_try_attack(void) {
    int8_t dx, dy;
    uint8_t nx, ny;
    uint8_t target;
    uint8_t roll;
    uint8_t dmg;
    uint8_t target_ac;
    uint8_t dam_dice, dam_sides;
    uint8_t max_hp;

    /* Determine pet damage dice based on pet type */
    if (player.pet_type == PET_CAT) {
        dam_dice = CAT_DICE;
        dam_sides = CAT_SIDES;
        max_hp = CAT_HP;
    } else {
        dam_dice = DOG_DICE;
        dam_sides = DOG_SIDES;
        max_hp = DOG_HP;
    }

    /* Check all 8 adjacent tiles for hostile monsters */
    for (dx = -1; dx <= 1; dx++) {
        for (dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;

            nx = monsters[pet_index].x + dx;
            ny = monsters[pet_index].y + dy;

            if (nx >= MAP_WIDTH || ny >= MAP_HEIGHT) continue;

            target = monster_at(nx, ny);
            if (target == 255) continue;
            if (target == pet_index) continue;

            /* Only attack hostile monsters (not peaceful) */
            if (monsters[target].status & MSTAT_PEACEFUL) continue;

            /* Roll to hit: 1d20 + PET_ATTACK_BONUS vs monster AC + 10 */
            target_ac = monster_types[monsters[target].type_id].ac;
            roll = rng_range(1, 20) + PET_ATTACK_BONUS;
            if (roll < target_ac + 10) {
                /* Miss */
                return;
            }

            /* Deal damage */
            dmg = rng_roll(dam_dice, dam_sides);
            if (dmg > 0) {
                monster_take_damage(target, dmg);
            }

            /* Only attack one monster per turn */
            return;
        }
    }
}

static void pet_try_eat(void) {
    uint8_t item_idx;
    uint8_t max_hp;

    item_idx = item_at(monsters[pet_index].x, monsters[pet_index].y);
    if (item_idx == 255) return;

    /* Check if it's a corpse (type 30) and food category */
    if (floor_items[item_idx].type_id != ITEM_CORPSE) return;
    if (item_types[floor_items[item_idx].type_id].category != ICAT_FOOD) return;

    /* Determine max HP */
    if (player.pet_type == PET_CAT) {
        max_hp = CAT_HP;
    } else {
        max_hp = DOG_HP;
    }

    /* Heal pet */
    monsters[pet_index].hp += PET_EAT_HEAL;
    if (monsters[pet_index].hp > max_hp) {
        monsters[pet_index].hp = max_hp;
    }

    /* Remove corpse from floor */
    item_remove_floor(item_idx);

    if (player.pet_type == PET_CAT) {
        ui_message("Your kitten eats");
        ui_message("the corpse.");
    } else {
        ui_message("Your puppy eats");
        ui_message("the corpse.");
    }
}
