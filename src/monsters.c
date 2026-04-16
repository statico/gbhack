#pragma bank 4

#include "monsters.h"
#include "dungeon.h"
#include "player.h"
#include "rng.h"
#include "sound.h"

extern const char *monster_name(uint8_t type_id);

Monster monsters[MAX_MONSTERS];

/* Forward declarations */
static void monster_move_toward(uint8_t idx, uint8_t tx, uint8_t ty);
static void monster_move_away(uint8_t idx, uint8_t tx, uint8_t ty);
static void monster_move_random(uint8_t idx);
static void monster_attack_player(uint8_t idx);
static uint8_t monster_adjacent_to_player(uint8_t idx);
static uint8_t monster_try_move(uint8_t idx, int8_t dx, int8_t dy);

void monsters_init(void) BANKED {
    uint8_t i;
    for (i = 0; i < MAX_MONSTERS; i++) {
        monsters[i].active = 0;
    }
}

uint8_t monster_spawn(uint8_t type_id, uint8_t x, uint8_t y) BANKED {
    uint8_t i;
    const MonsterType *mt;
    uint8_t cell;

    for (i = 0; i < MAX_MONSTERS; i++) {
        if (!monsters[i].active) {
            mt = &monster_types[type_id];
            monsters[i].active = 1;
            monsters[i].type_id = type_id;
            monsters[i].x = x;
            monsters[i].y = y;
            monsters[i].hp = mt->max_hp;
            monsters[i].status = 0;
            monsters[i].timer = 0;
            monsters[i].target_x = 0;
            monsters[i].target_y = 0;
            monsters[i].item1 = 0;
            monsters[i].item2 = 0;

            /* Shopkeeper starts peaceful */
            if (type_id == 22) {
                monsters[i].status |= MSTAT_PEACEFUL;
            }

            /* Set map flag */
            cell = dungeon_get_cell(x, y);
            dungeon_set_cell(x, y, cell | CELL_HAS_MONSTER);

            return i;
        }
    }
    return 255; /* no free slot */
}

void monsters_spawn_for_level(uint8_t level) BANKED {
    uint8_t count;
    uint8_t i;
    uint8_t type_id;
    uint8_t fx, fy;
    uint8_t candidates[25];
    uint8_t num_candidates;
    uint8_t max_level;

    count = rng_range(4, 8);
    max_level = level + 2;

    /* Build candidate list of monster types for this level */
    num_candidates = 0;
    for (i = 0; i < num_monster_types; i++) {
        /* Skip shopkeeper (22) from random spawning */
        if (i == 22) continue;
        if (monster_types[i].level <= max_level) {
            candidates[num_candidates] = i;
            num_candidates++;
        }
    }

    if (num_candidates == 0) return;

    for (i = 0; i < count; i++) {
        type_id = candidates[rng_range(0, num_candidates - 1)];
        dungeon_find_random_floor(&fx, &fy);

        /* Don't spawn on player or on another monster */
        if (fx == player.x && fy == player.y) continue;
        if (dungeon_get_cell(fx, fy) & CELL_HAS_MONSTER) continue;

        monster_spawn(type_id, fx, fy);
    }
}

uint8_t monster_at(uint8_t x, uint8_t y) BANKED {
    uint8_t i;
    for (i = 0; i < MAX_MONSTERS; i++) {
        if (monsters[i].active && monsters[i].x == x && monsters[i].y == y) {
            return i;
        }
    }
    return 255;
}

void monster_take_damage(uint8_t idx, uint8_t dmg) BANKED {
    if (dmg >= monsters[idx].hp) {
        monster_kill(idx);
    } else {
        monsters[idx].hp -= dmg;
    }
}

void monster_kill(uint8_t idx) BANKED {
    uint8_t cell;
    const MonsterType *mt;
    uint16_t xp_reward;

    mt = &monster_types[monsters[idx].type_id];

    /* Clear map flag */
    cell = dungeon_get_cell(monsters[idx].x, monsters[idx].y);
    dungeon_set_cell(monsters[idx].x, monsters[idx].y, cell & ~CELL_HAS_MONSTER);

    /* Award XP: monster level * 5 */
    xp_reward = (uint16_t)mt->level * 5;
    player_gain_xp(xp_reward);

    monsters[idx].active = 0;
}

void monsters_update(void) BANKED {
    uint8_t i;
    const MonsterType *mt;
    uint8_t behavior;
    uint8_t dist;
    int8_t ddx, ddy;

    for (i = 0; i < MAX_MONSTERS; i++) {
        if (!monsters[i].active) continue;

        mt = &monster_types[monsters[i].type_id];

        /* Speed: slow monsters act every other turn */
        if (mt->speed == SPEED_SLOW) {
            if (player.turns & 1) continue;
        }

        /* Asleep: skip entirely */
        if (monsters[i].status & MSTAT_ASLEEP) {
            /* Wake up if player is adjacent */
            ddx = (int8_t)player.x - (int8_t)monsters[i].x;
            ddy = (int8_t)player.y - (int8_t)monsters[i].y;
            if (ABS(ddx) <= 1 && ABS(ddy) <= 1) {
                monsters[i].status &= ~MSTAT_ASLEEP;
            }
            continue;
        }

        /* Paralyzed: decrement timer, skip */
        if (monsters[i].status & MSTAT_PARALYZED) {
            if (monsters[i].timer > 0) {
                monsters[i].timer--;
            }
            if (monsters[i].timer == 0) {
                monsters[i].status &= ~MSTAT_PARALYZED;
            }
            continue;
        }

        /* Regeneration */
        if (mt->flags & MFLAG_REGEN) {
            if (monsters[i].hp < mt->max_hp) {
                monsters[i].hp++;
            }
        }

        /* Peaceful monsters do nothing */
        if (monsters[i].status & MSTAT_PEACEFUL) {
            continue;
        }

        /* Determine effective behavior */
        behavior = mt->behavior;
        if (monsters[i].status & MSTAT_FLEEING) {
            behavior = AI_FLEEING;
        }
        if (monsters[i].status & MSTAT_CONFUSED) {
            behavior = AI_ERRATIC;
        }

        switch (behavior) {
        case AI_AGGRESSIVE:
            if (monster_adjacent_to_player(i)) {
                monster_attack_player(i);
            } else {
                monster_move_toward(i, player.x, player.y);
            }
            break;

        case AI_PASSIVE:
            /* Don't move, just sit there */
            break;

        case AI_ERRATIC:
            if (monster_adjacent_to_player(i)) {
                /* 50% chance to attack even when erratic */
                if (rng_range(0, 1)) {
                    monster_attack_player(i);
                } else {
                    monster_move_random(i);
                }
            } else {
                monster_move_random(i);
            }
            break;

        case AI_WANDER:
            /* Switch to aggressive if player within 6 tiles */
            ddx = (int8_t)player.x - (int8_t)monsters[i].x;
            ddy = (int8_t)player.y - (int8_t)monsters[i].y;
            dist = ABS(ddx) + ABS(ddy);
            if (dist <= 6) {
                /* Become aggressive */
                if (monster_adjacent_to_player(i)) {
                    monster_attack_player(i);
                } else {
                    monster_move_toward(i, player.x, player.y);
                }
            } else {
                monster_move_random(i);
            }
            break;

        case AI_FLEEING:
            monster_move_away(i, player.x, player.y);
            break;
        }

        /* Fast monsters get a second action */
        if (mt->speed == SPEED_FAST) {
            if (behavior == AI_AGGRESSIVE || behavior == AI_WANDER) {
                if (monster_adjacent_to_player(i)) {
                    monster_attack_player(i);
                } else if (behavior == AI_AGGRESSIVE) {
                    monster_move_toward(i, player.x, player.y);
                } else {
                    monster_move_random(i);
                }
            } else if (behavior == AI_ERRATIC) {
                monster_move_random(i);
            }
        }
    }
}

static uint8_t monster_adjacent_to_player(uint8_t idx) {
    int8_t ddx, ddy;
    ddx = (int8_t)player.x - (int8_t)monsters[idx].x;
    ddy = (int8_t)player.y - (int8_t)monsters[idx].y;
    if (ABS(ddx) <= 1 && ABS(ddy) <= 1 && (ddx || ddy)) {
        return 1;
    }
    return 0;
}

static void monster_attack_player(uint8_t idx) {
    uint8_t roll;
    uint8_t dmg;
    const MonsterType *mt;
    uint8_t extra;
    uint8_t nx, ny;
    uint8_t cell;

    mt = &monster_types[monsters[idx].type_id];

    /* Roll 1d20 + monster level, hit if >= player AC + 10 */
    roll = rng_range(1, 20) + mt->level;
    if (roll < (uint8_t)(player.ac + 10)) {
        /* Miss */
        return;
    }

    /* Damage: monster's dice */
    if (mt->damage_dice == 0) {
        dmg = 0;
    } else {
        dmg = rng_roll(mt->damage_dice, mt->damage_sides);
    }

    if (dmg > 0) {
        sound_play_sfx(SFX_ATTACK);
        player_set_death_cause(monster_name(monsters[idx].type_id));
        player_take_damage(dmg);
    }

    /* Special effects */
    if (mt->flags & MFLAG_POISON) {
        extra = rng_roll(1, 4);
        player_set_death_cause(monster_name(monsters[idx].type_id));
        player_take_damage(extra);
    }

    if (mt->flags & MFLAG_PARALYZE) {
        /* Player loses turns -- handled by caller checking paralysis timer */
        /* Store paralysis duration in player turns (2d4) */
        /* For now, just deal 0 damage but the effect needs turn system support */
    }

    if (mt->flags & MFLAG_DRAIN) {
        /* Drain 1 max HP */
        if (player.max_hp > 1) {
            player.max_hp--;
            if (player.hp > player.max_hp) {
                player.hp = player.max_hp;
            }
        }
    }

    if (mt->flags & MFLAG_STEAL) {
        /* Steal some gold */
        if (player.gold > 0) {
            extra = rng_range(5, 20);
            if (extra > player.gold) {
                extra = (uint8_t)player.gold;
            }
            player.gold -= extra;
            /* Nymph teleports away after stealing */
            monsters[idx].status |= MSTAT_FLEEING;
        }
    }

    if (mt->flags & MFLAG_TELEPORT) {
        /* Imp teleports to random location after attacking */
        if (rng_range(0, 2) == 0) {
            cell = dungeon_get_cell(monsters[idx].x, monsters[idx].y);
            dungeon_set_cell(monsters[idx].x, monsters[idx].y, cell & ~CELL_HAS_MONSTER);
            dungeon_find_random_floor(&nx, &ny);
            monsters[idx].x = nx;
            monsters[idx].y = ny;
            cell = dungeon_get_cell(nx, ny);
            dungeon_set_cell(nx, ny, cell | CELL_HAS_MONSTER);
        }
    }
}

static uint8_t monster_try_move(uint8_t idx, int8_t dx, int8_t dy) {
    uint8_t nx, ny;
    uint8_t cell;
    uint8_t old_cell;

    nx = monsters[idx].x + dx;
    ny = monsters[idx].y + dy;

    /* Bounds check */
    if (nx >= MAP_WIDTH || ny >= MAP_HEIGHT) return 0;

    /* Don't walk onto player */
    if (nx == player.x && ny == player.y) return 0;

    cell = dungeon_get_cell(nx, ny);

    /* Must be passable and unoccupied */
    if (!CELL_IS_PASSABLE(cell)) return 0;
    if (cell & CELL_HAS_MONSTER) return 0;

    /* Clear old cell flag */
    old_cell = dungeon_get_cell(monsters[idx].x, monsters[idx].y);
    dungeon_set_cell(monsters[idx].x, monsters[idx].y, old_cell & ~CELL_HAS_MONSTER);

    /* Move */
    monsters[idx].x = nx;
    monsters[idx].y = ny;

    /* Set new cell flag */
    dungeon_set_cell(nx, ny, cell | CELL_HAS_MONSTER);

    return 1;
}

static void monster_move_toward(uint8_t idx, uint8_t tx, uint8_t ty) {
    int8_t dx, dy;

    dx = 0;
    dy = 0;

    if (tx > monsters[idx].x) dx = 1;
    else if (tx < monsters[idx].x) dx = -1;

    if (ty > monsters[idx].y) dy = 1;
    else if (ty < monsters[idx].y) dy = -1;

    /* Try diagonal first */
    if (dx && dy) {
        if (monster_try_move(idx, dx, dy)) return;
    }

    /* Try cardinal directions */
    if (dx) {
        if (monster_try_move(idx, dx, 0)) return;
    }
    if (dy) {
        if (monster_try_move(idx, 0, dy)) return;
    }
}

static void monster_move_away(uint8_t idx, uint8_t tx, uint8_t ty) {
    int8_t dx, dy;

    dx = 0;
    dy = 0;

    /* Move in opposite direction from target */
    if (tx > monsters[idx].x) dx = -1;
    else if (tx < monsters[idx].x) dx = 1;

    if (ty > monsters[idx].y) dy = -1;
    else if (ty < monsters[idx].y) dy = 1;

    if (dx && dy) {
        if (monster_try_move(idx, dx, dy)) return;
    }
    if (dx) {
        if (monster_try_move(idx, dx, 0)) return;
    }
    if (dy) {
        if (monster_try_move(idx, 0, dy)) return;
    }

    /* If can't flee, try random */
    monster_move_random(idx);
}

static void monster_move_random(uint8_t idx) {
    int8_t dx, dy;
    uint8_t dir;

    dir = rng_range(0, 7);

    switch (dir) {
    case 0: dx =  0; dy = -1; break;  /* N */
    case 1: dx =  0; dy =  1; break;  /* S */
    case 2: dx =  1; dy =  0; break;  /* E */
    case 3: dx = -1; dy =  0; break;  /* W */
    case 4: dx =  1; dy = -1; break;  /* NE */
    case 5: dx = -1; dy = -1; break;  /* NW */
    case 6: dx =  1; dy =  1; break;  /* SE */
    case 7: dx = -1; dy =  1; break;  /* SW */
    default: dx = 0; dy = 0; break;
    }

    monster_try_move(idx, dx, dy);
}
