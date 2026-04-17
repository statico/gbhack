#include "player.h"
#include "dungeon.h"
#include "inventory.h"
#include "monsters.h"
#include "pet.h"
#include "render.h"
#include "rng.h"
#include "sound.h"
#include "ui.h"

/* Monster name lookup (defined in monsters_data.c bank 0) */
extern const char *monster_name(uint8_t type_id);

/* Death cause tracking */
static const char *death_cause = "something";
const char *player_get_death_cause(void) { return death_cause; }
void player_set_death_cause(const char *cause) { death_cause = cause; }

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

uint8_t player_move(int8_t dx, int8_t dy) {
    uint8_t nx, ny;
    uint8_t cell;
    uint8_t terrain;
    uint8_t midx;

    nx = player.x + dx;
    ny = player.y + dy;

    /* Bounds check */
    if (nx >= MAP_WIDTH || ny >= MAP_HEIGHT) {
        return 1;
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
            return 0;
        }
    }

    /* Closed door: open it (costs a turn) */
    if (terrain == TERRAIN_DOOR_CLOSED) {
        dungeon_open_door(nx, ny);
        sound_play_sfx(SFX_DOOR);
        player.nutrition--;
        player.turns++;
        return 0;
    }

    /* Check passability */
    if (!CELL_IS_PASSABLE(cell)) {
        /* Mining: if pickaxe equipped and target is a wall, dig it */
        if (terrain == TERRAIN_WALL) {
            uint8_t pick_slot;
            pick_slot = inventory_get_equipped_pickaxe();
            if (pick_slot != 255) {
                dungeon_set_cell(nx, ny, TERRAIN_CORRIDOR);
                sound_play_sfx(SFX_HIT);
                inventory[pick_slot].quantity--;
                if (inventory[pick_slot].quantity == 0) {
                    inventory[pick_slot].flags &= ~IFLAG_EQUIPPED;
                    inventory_remove(pick_slot);
                    ui_message("Your pickaxe breaks!");
                } else {
                    ui_message("You dig.");
                }
                player.nutrition--;
                player.turns++;
                return 0;
            }
        }
        return 1;
    }

    /* Move player */
    player.x = nx;
    player.y = ny;
    sound_play_sfx(SFX_STEP);
    player.nutrition--;
    player.turns++;
    return 0;
}

static void player_attack_monster(uint8_t idx) {
    uint8_t roll;
    uint8_t dmg;
    int8_t target_ac;
    const MonsterType *mt;

    /* Don't attack your own pet — swap positions silently */
    if (idx == pet_index) {
        {
            uint8_t px, py, mx, my, cell;
            px = player.x;
            py = player.y;
            mx = monsters[idx].x;
            my = monsters[idx].y;
            /* Clear monster from old cell */
            cell = dungeon_get_cell(mx, my);
            dungeon_set_cell(mx, my, cell & ~CELL_HAS_MONSTER);
            /* Move player to monster's position */
            player.x = mx;
            player.y = my;
            /* Move monster to player's old position */
            monsters[idx].x = px;
            monsters[idx].y = py;
            cell = dungeon_get_cell(px, py);
            dungeon_set_cell(px, py, cell | CELL_HAS_MONSTER);
        }
        return;
    }

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

    /* Bump animation: player tile lunges at monster */
    render_bump_attack(player.x, player.y,
                       monsters[idx].x, monsters[idx].y);
    sound_play_sfx(SFX_HIT);

    /* Show hit message */
    {
        const char *mname;
        mname = monster_name(monsters[idx].type_id);
        if (monsters[idx].hp <= dmg) {
            /* This will kill it */
            ui_message("You kill the");
            ui_message(mname);
        } else {
            ui_message("You hit the");
            ui_message(mname);
        }
    }

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
    uint8_t prev_state;

    if (player.nutrition > 0) {
        player.nutrition--;
    }

    prev_state = player.hunger_state;

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
        death_cause = "starvation";
        player.hp = 0;
    }

    /* Announce hunger milestones */
    if (player.hunger_state != prev_state) {
        if (player.hunger_state == HUNGER_HUNGRY) {
            ui_message("You are hungry.");
        } else if (player.hunger_state == HUNGER_WEAK) {
            ui_message("You feel weak.");
        } else if (player.hunger_state == HUNGER_FAINTING) {
            ui_message("You are starving!");
        }
    }
}

uint8_t player_is_dead(void) {
    return (player.hp == 0) ? 1 : 0;
}
