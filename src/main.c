#include "common.h"
#include "player.h"
#include "dungeon.h"
#include "monsters.h"
#include "items.h"
#include "inventory.h"
#include "render.h"
#include "fov.h"
#include "input.h"
#include "ui.h"
#include "rng.h"
#include "save.h"
#include "sound.h"
#include "shop.h"
#include "pet.h"
#include "../res/tiles.h"
#include "../res/title_bg.h"
#include <gb/cgb.h>

/* ------------------------------------------------------------------ */
/* Direction lookup tables                                             */
/* ------------------------------------------------------------------ */

static const int8_t dir_dx[] = {
    /* DIR_NONE, N,  S,  E,  W, NE, NW, SE, SW */
       0,       0,  0,  1, -1,  1, -1,  1, -1
};
static const int8_t dir_dy[] = {
    /* DIR_NONE, N,  S,  E,  W, NE, NW, SE, SW */
       0,      -1,  1,  0,  0, -1, -1,  1,  1
};

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */

static void intro_screen(void);
static void title_screen(void);
static void new_game(void);
static void game_loop(void);
static void handle_action(uint8_t action);
static void death_sequence(void);
static void win_sequence(void);
static void descend_stairs(void);
static void ascend_stairs(void);

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

static uint8_t game_state;

/* ------------------------------------------------------------------ */
/* Title screen                                                        */
/* ------------------------------------------------------------------ */

/* Helper: write a string to BG map at (x,y) using VRAM bank 1 font tiles */
static void title_draw_text(uint8_t x, uint8_t y, const char *str, uint8_t pal) {
    uint8_t i;
    uint8_t attr;
    attr = pal | 0x08;  /* palette + VRAM bank 1 bit (BG attr bit 3) */

    VBK_REG = 0;
    for (i = 0; str[i] != 0; i++) {
        set_bkg_tile_xy(x + i, y, (uint8_t)str[i]);
    }
    VBK_REG = 1;
    for (i = 0; str[i] != 0; i++) {
        set_bkg_tile_xy(x + i, y, attr);
    }
    VBK_REG = 0;
}

/* ------------------------------------------------------------------ */
/* Fade helpers for palette transitions                                */
/* ------------------------------------------------------------------ */
static void fade_set_palettes(const uint16_t *target, uint8_t num_pal, uint8_t step, uint8_t max_step) {
    uint16_t faded[32];
    uint8_t total;
    uint8_t c;

    total = num_pal * 4;
    if (total > 32) total = 32;

    for (c = 0; c < total; c++) {
        uint8_t r, g, b;
        r = target[c] & 0x1F;
        g = (target[c] >> 5) & 0x1F;
        b = (target[c] >> 10) & 0x1F;
        faded[c] = ((uint16_t)((r * step) / max_step)) |
                   ((uint16_t)((g * step) / max_step) << 5) |
                   ((uint16_t)((b * step) / max_step) << 10);
    }
    set_bkg_palette(0, num_pal, faded);
}

static void fade_from_black(const uint16_t *target, uint8_t num_pal, uint8_t steps) {
    uint8_t s;
    for (s = 0; s <= steps; s++) {
        wait_vbl_done();
        fade_set_palettes(target, num_pal, s, steps);
    }
}

static void fade_to_black(const uint16_t *target, uint8_t num_pal, uint8_t steps) {
    uint8_t s;
    for (s = steps; s > 0; s--) {
        wait_vbl_done();
        fade_set_palettes(target, num_pal, s, steps);
    }
    /* Final frame: all black */
    {
        uint16_t black[32];
        uint8_t c;
        uint8_t total;
        total = num_pal * 4;
        if (total > 32) total = 32;
        for (c = 0; c < total; c++) black[c] = 0;
        wait_vbl_done();
        set_bkg_palette(0, num_pal, black);
    }
}

/* ------------------------------------------------------------------ */
/* Intro / credits screen                                              */
/* ------------------------------------------------------------------ */

/* Palette for intro screen: dark bg, light text */
static const uint16_t intro_palette[4] = {
    RGB( 2,  2,  2),  /* color 0: near-black bg */
    RGB( 2,  2,  2),  /* color 1: unused */
    RGB( 2,  2,  2),  /* color 2: unused */
    RGB(31, 31, 31)   /* color 3: white text */
};

/* Sword icon tile index */
#define TILE_INTRO_SWORD 14

static void intro_screen(void) {
    uint8_t x, y;
    uint16_t wait_frames;

    DISPLAY_OFF;

    /* Set 8000 addressing mode */
    LCDC_REG |= LCDCF_BG8000;

    /* Load gameplay tileset (has font + sword icon) */
    tiles_load();

    /* Start with black palette */
    {
        uint16_t black[4];
        black[0] = 0; black[1] = 0; black[2] = 0; black[3] = 0;
        set_bkg_palette(0, 1, black);
    }

    /* Clear entire screen to dark background */
    VBK_REG = 0;
    for (y = 0; y < 18; y++) {
        for (x = 0; x < 20; x++) {
            set_bkg_tile_xy(x, y, ' ');
        }
    }
    /* Set all tiles to palette 0 */
    VBK_REG = 1;
    for (y = 0; y < 18; y++) {
        for (x = 0; x < 20; x++) {
            set_bkg_tile_xy(x, y, 0);
        }
    }
    VBK_REG = 0;

    /* Draw centered credits text */
    /*  Row 4: sword icon (centered) */
    set_bkg_tile_xy(10, 4, TILE_INTRO_SWORD);

    /*  Row 6: "GBHack v1.0.0" (14 chars, x=3) */
    ui_draw_text(3, 6, "GBHack v1.0.0", 0);

    /*  Row 7-8: "by @statico and Claude Code" */
    ui_draw_text(3, 7, "by @statico and", 0);
    ui_draw_text(5, 8, "Claude Code", 0);

    /*  Row 10: "Based on NetHack" (16 chars, x=2) */
    ui_draw_text(2, 10, "Based on NetHack", 0);

    /*  Row 12-14: music credits */
    ui_draw_text(6, 12, "Music by", 0);
    ui_draw_text(4, 13, "Beatscribe &", 0);
    ui_draw_text(3, 14, "Yoki (Trominal)", 0);

    SCX_REG = 0;
    SCY_REG = 0;
    HIDE_WIN;
    SHOW_BKG;
    DISPLAY_ON;

    /* Fade in from black */
    fade_from_black(intro_palette, 1, 8);

    /* Wait ~3 seconds (180 frames at 60fps) or until START is pressed */
    wait_frames = 0;
    while (wait_frames < 180) {
        wait_vbl_done();
        input_update();
        if (joy_pressed & J_START) {
            break;
        }
        wait_frames++;
    }

    /* Fade out to black */
    fade_to_black(intro_palette, 1, 8);
}

/* ------------------------------------------------------------------ */
/* Title screen                                                        */
/* ------------------------------------------------------------------ */

static void title_screen(void) {
    uint8_t saved_bank;

    DISPLAY_OFF;

    /* Set 8000 addressing mode before loading tiles */
    LCDC_REG |= LCDCF_BG8000;

    /* Load title BG image tiles, map, and palettes from bank 7 */
    saved_bank = CURRENT_BANK;
    SWITCH_ROM(BANK(title_bg));

    /* Load image tiles into VRAM bank 0 only */
    VBK_REG = 0;
    set_bkg_data(0, title_bg_TILE_COUNT, title_bg_tiles);

    /* Start with black palettes for fade-in */
    {
        uint16_t black[32];
        uint8_t c;
        for (c = 0; c < 32; c++) black[c] = 0;
        set_bkg_palette(0, 8, black);
    }

    /* Write tile map */
    VBK_REG = 0;
    set_bkg_tiles(0, 0, 20, 18, title_bg_map);

    /* Write attributes with VRAM bank bit cleared (all image tiles in bank 0).
     * This frees bank 1 for font overlay on the PRESS START row. */
    VBK_REG = 1;
    {
        uint8_t attr_row[20];
        uint8_t ty, tx;
        for (ty = 0; ty < 18; ty++) {
            for (tx = 0; tx < 20; tx++) {
                attr_row[tx] = title_bg_map_attributes[ty * 20 + tx] & 0x07;
            }
            set_bkg_tiles(0, ty, 20, 1, attr_row);
        }
    }
    VBK_REG = 0;

    SWITCH_ROM(saved_bank);

    /* Load font tiles into VRAM bank 1 for text overlay */
    VBK_REG = 1;
    tiles_load_font_bank1();
    VBK_REG = 0;

    /* "Press START" on row 16: font tiles from bank 1, palette 7 */
    title_draw_text(5, 16, "PRESS START", 7);

    SCX_REG = 0;
    SCY_REG = 0;
    HIDE_WIN;
    SHOW_BKG;
    DISPLAY_ON;

    sound_init();
    sound_play_music(MUSIC_TITLE);

    /* Fade in from black */
    SWITCH_ROM(BANK(title_bg));
    fade_from_black(title_bg_palettes, 8, 8);
    SWITCH_ROM(saved_bank);

    /* Wait for START with blinking "PRESS START" text.
     * Use tile-based blink to avoid disturbing bg palette 7. */
    {
        uint8_t frame;
        uint8_t visible;
        uint8_t blink_i;

        frame = 0;
        visible = 1;

        while (1) {
            wait_vbl_done();
            input_update();
            if (joy_pressed & J_START) {
                break;
            }

            frame++;
            /* Toggle visibility every 30 frames (~0.5 sec) */
            if ((frame % 30) == 0) {
                visible = !visible;
                if (visible) {
                    title_draw_text(5, 16, "PRESS START", 7);
                } else {
                    /* Clear the text row */
                    VBK_REG = 0;
                    for (blink_i = 5; blink_i < 16; blink_i++) {
                        set_bkg_tile_xy(blink_i, 16, 0);
                    }
                    VBK_REG = 0;
                }
            }
        }
    }

    /* Fade out to black before transitioning */
    SWITCH_ROM(BANK(title_bg));
    fade_to_black(title_bg_palettes, 8, 8);
    SWITCH_ROM(saved_bank);
}

/* ------------------------------------------------------------------ */
/* New game setup                                                      */
/* ------------------------------------------------------------------ */

static void new_game(void) {
    player_init();
    items_init();
    items_shuffle_appearances();
    inventory_init();
    monsters_init();

    /* Generate first dungeon level */
    dungeon_generate(1);
    monsters_spawn_for_level(1);
    items_spawn_for_level(1);
    shop_init();

    /* Stock any shop rooms on this level */
    {
        uint8_t ri;
        for (ri = 0; ri < dungeon_num_rooms; ri++) {
            if (dungeon_rooms[ri].type == 1) {
                shop_stock_room(ri);
            }
        }
    }

    /* Place player at up staircase */
    player.x = stairs_up_x;
    player.y = stairs_up_y;

    /* Pet choice */
    {
        uint8_t pet_choice;
        pet_choice = ui_yes_no("Start w/ cat?");
        if (pet_choice) {
            pet_init(1);
        } else {
            pet_init(2);
        }
    }

    /* Initial FOV */
    fov_clear();
    fov_calculate(player.x, player.y);

    render_init();
    render_full_redraw();
    sound_play_music(MUSIC_DUNGEON1);
    ui_message("Welcome to GBHack!");
}

/* ------------------------------------------------------------------ */
/* Stair traversal                                                     */
/* ------------------------------------------------------------------ */

static void descend_stairs(void) {
    if (player.dungeon_level >= MAX_DUNGEON_LEVELS) {
        ui_message("No deeper!");
        return;
    }

    player.dungeon_level++;
    dungeon_generate(player.dungeon_level);
    monsters_init();
    monsters_spawn_for_level(player.dungeon_level);
    items_spawn_for_level(player.dungeon_level);
    shop_init();

    /* Stock shop rooms */
    {
        uint8_t ri;
        for (ri = 0; ri < dungeon_num_rooms; ri++) {
            if (dungeon_rooms[ri].type == 1) {
                shop_stock_room(ri);
            }
        }
    }

    player.x = stairs_up_x;
    player.y = stairs_up_y;
    fov_clear();
    fov_calculate(player.x, player.y);
    render_full_redraw();
    sound_play_sfx(SFX_STAIRS);

    /* Change music for deeper levels */
    if (player.dungeon_level >= 15) {
        sound_play_music(MUSIC_BOSS);
    } else if (player.dungeon_level >= 8) {
        sound_play_music(MUSIC_DUNGEON2);
    }

    ui_message("You descend.");
}

static void ascend_stairs(void) {
    uint8_t amulet_slot;

    if (player.dungeon_level <= 1) {
        /* Check if player has the Amulet of Yendor */
        amulet_slot = inventory_find(35);
        if (amulet_slot != 255) {
            /* Win condition! */
            game_state = STATE_WIN;
            return;
        }
        ui_message("The exit is sealed.");
        return;
    }

    player.dungeon_level--;
    dungeon_generate(player.dungeon_level);
    monsters_init();
    monsters_spawn_for_level(player.dungeon_level);
    items_spawn_for_level(player.dungeon_level);
    player.x = stairs_down_x;
    player.y = stairs_down_y;
    fov_clear();
    fov_calculate(player.x, player.y);
    render_full_redraw();

    /* Update music for new level */
    if (player.dungeon_level >= 15) {
        sound_play_music(MUSIC_BOSS);
    } else if (player.dungeon_level >= 8) {
        sound_play_music(MUSIC_DUNGEON2);
    } else {
        sound_play_music(MUSIC_DUNGEON1);
    }

    ui_message("You ascend.");
}

/* ------------------------------------------------------------------ */
/* Handle action menu result                                           */
/* ------------------------------------------------------------------ */

static void handle_action(uint8_t action) {
    uint8_t slot;
    uint8_t dir;

    switch (action) {
    case ACTION_PICKUP:
        inventory_pickup();
        break;

    case ACTION_EAT:
        slot = ui_inventory_screen(ICAT_FOOD);
        if (slot != 255) {
            inventory_use(slot);
        }
        break;

    case ACTION_QUAFF:
        slot = ui_inventory_screen(ICAT_POTION);
        if (slot != 255) {
            inventory_use(slot);
        }
        break;

    case ACTION_READ:
        slot = ui_inventory_screen(ICAT_SCROLL);
        if (slot != 255) {
            inventory_use(slot);
        }
        break;

    case ACTION_ZAP:
        slot = ui_inventory_screen(ICAT_WAND);
        if (slot != 255) {
            dir = ui_direction_prompt();
            if (dir != DIR_NONE) {
                /* Zap wand in direction -- use the item (consumes charge) */
                inventory_use(slot);
            }
        }
        break;

    case ACTION_DROP:
        slot = ui_inventory_screen(255);  /* show all categories */
        if (slot != 255) {
            inventory_drop(slot);
        }
        break;

    case ACTION_WAIT:
        /* Intentional no-op: just pass the turn */
        break;

    case ACTION_SAVE_QUIT:
        save_game();
        ui_message("Game saved.");
        ui_show_messages();
        wait_vbl_done();
        wait_vbl_done();
        reset();
        break;

    case ACTION_INVENTORY:
        slot = ui_inventory_screen(255);  /* view inventory, show all */
        if (slot != 255) {
            inventory_use(slot);
        }
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Death sequence                                                      */
/* ------------------------------------------------------------------ */

static void death_sequence(void) {
    uint16_t score;

    sound_stop_music();
    sound_play_sfx(SFX_DEATH);
    sound_play_music(MUSIC_DEATH);
    ui_message("You die...");
    ui_show_messages();

    save_write_bones();
    save_delete();

    score = player.xp + player.gold;
    save_write_hiscore(score);

    /* Wait for player to acknowledge */
    render_update();
    render_status_bar();

    ui_draw_text(3, 5, "-- GAME OVER --", PAL_UI);

    /* Show cause of death */
    ui_draw_text(2, 7, "Killed by:", PAL_UI);
    ui_draw_text(3, 8, player_get_death_cause(), PAL_UI);

    /* Build score display */
    {
        uint8_t buf[16];
        uint16_t val;
        uint8_t pos;
        uint8_t digits[5];
        uint8_t d;

        buf[0] = 'S';
        buf[1] = 'c';
        buf[2] = 'o';
        buf[3] = 'r';
        buf[4] = 'e';
        buf[5] = ':';
        buf[6] = ' ';

        val = score;
        pos = 0;
        do {
            digits[pos] = val % 10;
            val /= 10;
            pos++;
        } while (val > 0);

        d = 7;
        while (pos > 0) {
            pos--;
            buf[d] = '0' + digits[pos];
            d++;
        }
        buf[d] = '\0';

        ui_draw_text(4, 10, (const char *)buf, PAL_UI);
    }

    ui_draw_text(3, 13, "Press START", PAL_UI);

    /* Wait for START to return to title */
    while (1) {
        wait_vbl_done();
        input_update();
        if (joy_pressed & J_START) {
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Win sequence                                                        */
/* ------------------------------------------------------------------ */

static void win_sequence(void) {
    uint16_t score;

    save_delete();

    score = player.xp + player.gold + 5000;
    save_write_hiscore(score);

    sound_stop_music();
    sound_play_music(MUSIC_VICTORY);
    render_init();
    ui_draw_text(3, 3, "You escaped with", PAL_UI);
    ui_draw_text(2, 4, "the Amulet of Yendor!", PAL_UI);
    ui_draw_text(4, 6, "YOU WIN!", PAL_UI);

    {
        uint8_t buf[16];
        uint16_t val;
        uint8_t pos;
        uint8_t digits[5];
        uint8_t d;

        buf[0] = 'S';
        buf[1] = 'c';
        buf[2] = 'o';
        buf[3] = 'r';
        buf[4] = 'e';
        buf[5] = ':';
        buf[6] = ' ';

        val = score;
        pos = 0;
        do {
            digits[pos] = val % 10;
            val /= 10;
            pos++;
        } while (val > 0);

        d = 7;
        while (pos > 0) {
            pos--;
            buf[d] = '0' + digits[pos];
            d++;
        }
        buf[d] = '\0';

        ui_draw_text(4, 8, (const char *)buf, PAL_UI);
    }

    ui_draw_text(3, 11, "Press START", PAL_UI);

    while (1) {
        wait_vbl_done();
        input_update();
        if (joy_pressed & J_START) {
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Main game loop                                                      */
/* ------------------------------------------------------------------ */

static void game_loop(void) {
    uint8_t acted;
    uint8_t dir;
    uint8_t action;
    int8_t dx, dy;
    uint8_t cell;
    uint8_t terrain;
    uint8_t prev_x, prev_y;

    game_state = STATE_GAMEPLAY;

    while (game_state == STATE_GAMEPLAY) {
        /* Render current state */
        render_update();
        render_status_bar();
        ui_show_messages();

        /* Wait for input that consumes a turn */
        acted = 0;
        prev_x = player.x;
        prev_y = player.y;
        while (!acted) {
            wait_vbl_done();
            input_update();

            /* Check directional movement */
            dir = input_get_direction();
            if (dir != DIR_NONE) {
                dx = dir_dx[dir];
                dy = dir_dy[dir];
                prev_x = player.x;
                prev_y = player.y;
                player_move(dx, dy);
                acted = 1;
                break;
            }

            /* B button without D-pad: contextual action */
            if ((joy_pressed & J_B) && !(joy_current & (J_UP | J_DOWN | J_LEFT | J_RIGHT))) {
                /* If standing on an item, pick it up */
                uint8_t floor_idx;
                floor_idx = item_at(player.x, player.y);
                if (floor_idx != 255) {
                    inventory_pickup();
                    sound_play_sfx(SFX_PICKUP);
                } else {
                    /* No item — rest */
                    ui_message("You rest.");
                    player.turns++;
                }
                acted = 1;
                break;
            }

            /* A button: action menu */
            if (joy_pressed & J_A) {
                action = ui_action_menu();
                if (action != ACTION_NONE) {
                    handle_action(action);
                    /* Most actions consume a turn.
                     * Viewing inventory without using is free.
                     * Save+quit resets, so acted doesn't matter. */
                    if (action != ACTION_INVENTORY) {
                        acted = 1;
                    }
                    render_full_redraw();
                }
                break;
            }

            /* START: character sheet (free action) */
            if (joy_pressed & J_START) {
                ui_character_sheet();
                render_full_redraw();
                break;
            }

            /* SELECT: menu (free action) */
            if (joy_pressed & J_SELECT) {
                uint8_t sel_result;
                sel_result = ui_select_menu();
                if (sel_result == SEL_MENU_QUIT) {
                    save_game();
                    ui_message("Game saved.");
                    ui_show_messages();
                    wait_vbl_done();
                    wait_vbl_done();
                    reset();
                }
                render_full_redraw();
                break;
            }
        }

        if (!acted) {
            continue;
        }

        /* --- Turn processing --- */

        /* Check for items underfoot after moving */
        if (player.x != prev_x || player.y != prev_y) {
            uint8_t floor_idx;
            floor_idx = item_at(player.x, player.y);
            if (floor_idx != 255) {
                uint8_t tid;
                tid = floor_items[floor_idx].type_id;
                if (item_types[tid].category == ICAT_GOLD) {
                    ui_message("You see gold.");
                } else {
                    ui_message(item_appearance_name(tid));
                }
            }
        }

        /* Auto-clear stale messages */
        ui_message_tick(player.turns);

        /* Update hunger */
        player_update_hunger();

        /* Monster turns */
        monsters_update();

        /* Pet turn */
        pet_update();

        /* Shop detection */
        shop_check_enter();

        /* Auto-save every 50 turns */
        if ((player.turns % 50) == 0 && player.turns > 0) {
            save_game();
        }

        /* Check death */
        if (player_is_dead()) {
            death_sequence();
            game_state = STATE_GAMEOVER;
            break;
        }

        /* Check stairs */
        cell = dungeon_get_cell(player.x, player.y);
        terrain = CELL_TERRAIN(cell);

        if (terrain == TERRAIN_STAIRS_DOWN) {
            /* Only descend if player actually moved here this turn */
            if (player.x != prev_x || player.y != prev_y) {
                if (ui_yes_no("Descend?")) {
                    descend_stairs();
                }
            }
        }

        if (terrain == TERRAIN_STAIRS_UP) {
            if (player.x != prev_x || player.y != prev_y) {
                if (ui_yes_no("Ascend?")) {
                    ascend_stairs();
                    if (game_state == STATE_WIN) {
                        break;
                    }
                }
            }
        }

        /* Recalculate FOV */
        fov_calculate(player.x, player.y);
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

void main(void) {
    uint8_t has_save;
    uint8_t loaded;

    while (1) {
        /* Seed RNG from hardware divider */
        rng_seed(DIV_REG | ((uint16_t)DIV_REG << 8));

        /* Intro credits screen, then title screen */
        intro_screen();
        title_screen();

        /* Add some more entropy after player pressed start */
        rng_seed(rng_next() ^ DIV_REG);

        /* Reload gameplay tileset and palettes so UI rendering works */
        render_init();

        /* Check for existing save */
        loaded = 0;
        has_save = save_load();
        if (has_save) {
            if (ui_yes_no("Continue?")) {
                /* Save already loaded by save_load() */
                loaded = 1;
                fov_clear();
                fov_calculate(player.x, player.y);
                render_full_redraw();

                /* Start dungeon music appropriate for restored level */
                if (player.dungeon_level >= 15) {
                    sound_play_music(MUSIC_BOSS);
                } else if (player.dungeon_level >= 8) {
                    sound_play_music(MUSIC_DUNGEON2);
                } else {
                    sound_play_music(MUSIC_DUNGEON1);
                }

                ui_message("Welcome back.");
            } else {
                save_delete();
            }
        }

        if (!loaded) {
            new_game();
        }

        /* Run the game */
        game_loop();

        /* Handle win state */
        if (game_state == STATE_WIN) {
            win_sequence();
        }

        /* Loop back to title screen */
    }
}
