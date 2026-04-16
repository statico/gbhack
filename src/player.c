#include "player.h"
#include "dungeon.h"
#include "monsters.h"
#include "rng.h"
#include "sound.h"

Player player;

/* XP thresholds for levels 2..10+ (index = level - 2) */
static const uint16_t xp_thresholds[] = {
    20, 50, 100, 200, 400, 800, 1600, 3200, 6400
};
#define NUM_XP_THRESHOLDS 9

/* Forward declarations for combat helpers */
static void player_attack_monster(uint8_t idx);

void player_init(void) {
    player.x = 0;
    player.y = 0;
    player.hp = 20;
    player.max_hp = 20;
    player.ac = 10;
    player.strength = 10;
    player.level = 1;
    player.xp = 0;
    player.gold = 0;
    player.nutrition = 900;
    player.dungeon_level = 1;
    player.hunger_state = HUNGER_NORMAL;
    player.turns = 0;
    player.prayer_cooldown = 0;
    player.pet_type = 0;
    player.shopkeeper_hostile = 0;
}

void player_move(int8_t dx, int8_t dy) {
    uint8_t nx, ny;
    uint8_t cell;
    uint8_t terrain;
    uint8_t midx;

    nx = player.x + dx;
    ny = player.y + dy;

    /* Bounds check */
    if (nx >= MAP_WIDTH || ny >= MAP_HEIGHT) {
        return;
    }

    cell = dungeon_get_cell(nx, ny);
    terrain = CELL_TERRAIN(cell);

    /* Check for monster at target cell */
    if (cell & CELL_HAS_MONSTER) {
        midx = monster_at(nx, ny);
        if (midx != 255) {
            player_attack_monster(midx);
            player.nutrition--;
            player.turns++;
            return;
        }
    }

    /* Closed door: open it (costs a turn) */
    if (terrain == TERRAIN_DOOR_CLOSED) {
        dungeon_open_door(nx, ny);
        sound_play_sfx(SFX_DOOR);
        player.nutrition--;
        player.turns++;
        return;
    }

    /* Check passability */
    if (!CELL_IS_PASSABLE(cell)) {
        return;
    }

    /* Move player */
    player.x = nx;
    player.y = ny;
    player.nutrition--;
    player.turns++;
}

static void player_attack_monster(uint8_t idx) {
    uint8_t roll;
    uint8_t dmg;
    int8_t target_ac;
    const MonsterType *mt;

    mt = &monster_types[monsters[idx].type_id];
    target_ac = mt->ac;

    /* Roll 1d20 + player level, hit if >= monster AC + 10 */
    roll = rng_range(1, 20) + player.level;
    if (roll < (uint8_t)(target_ac + 10)) {
        /* Miss */
        sound_play_sfx(SFX_MISS);
        return;
    }

    /* Damage: bare hands 1d4 + strength/4 bonus */
    dmg = rng_roll(1, 4) + (player.strength >> 2);
    if (dmg == 0) {
        dmg = 1;
    }

    sound_play_sfx(SFX_HIT);
    monster_take_damage(idx, dmg);

    /* If monster was passive, make it aggressive */
    if ((monsters[idx].status & MSTAT_PEACEFUL) && monsters[idx].active) {
        monsters[idx].status &= ~MSTAT_PEACEFUL;
    }
}

void player_take_damage(uint8_t dmg) {
    if (dmg >= player.hp) {
        player.hp = 0;
    } else {
        player.hp -= dmg;
    }
}

void player_heal(uint8_t amount) {
    uint8_t new_hp;

    new_hp = player.hp + amount;
    if (new_hp > player.max_hp) {
        new_hp = player.max_hp;
    }
    player.hp = new_hp;
}

void player_gain_xp(uint16_t xp) {
    uint8_t thresh_idx;
    uint8_t roll;

    player.xp += xp;

    /* Check for level up */
    while (player.level <= 10) {
        thresh_idx = player.level - 1; /* level 1 -> index 0 -> threshold for level 2 */
        if (thresh_idx >= NUM_XP_THRESHOLDS) {
            break;
        }
        if (player.xp < xp_thresholds[thresh_idx]) {
            break;
        }

        /* Level up */
        player.level++;
        roll = rng_range(2, 6);
        player.max_hp += roll;
        player.hp = player.max_hp;

        /* 50% chance of +1 strength */
        if (rng_range(0, 1)) {
            player.strength++;
        }
    }
}

void player_update_hunger(void) {
    if (player.nutrition > 0) {
        player.nutrition--;
    }

    if (player.nutrition > 1000) {
        player.hunger_state = HUNGER_SATIATED;
    } else if (player.nutrition > 300) {
        player.hunger_state = HUNGER_NORMAL;
    } else if (player.nutrition > 150) {
        player.hunger_state = HUNGER_HUNGRY;
    } else if (player.nutrition > 50) {
        player.hunger_state = HUNGER_WEAK;
    } else if (player.nutrition > 0) {
        player.hunger_state = HUNGER_FAINTING;
    } else {
        player.hunger_state = HUNGER_STARVED;
        player.hp = 0;
    }
}

uint8_t player_is_dead(void) {
    return (player.hp == 0) ? 1 : 0;
}
