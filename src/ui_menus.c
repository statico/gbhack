#include "ui.h"
#include "ui_internal.h"
#include "input.h"
#include "inventory.h"
#include "items.h"
#include "player.h"
#include "render.h"
#include "sound.h"

/* Banked UI screens: action menu, direction prompt, character sheet,
   message history, help, select menu, yes/no, pet choice.
   Kept out of bank 0 to prevent CODE+HOME overflow.

   All functions here are BANKED — callers (mostly main.c in bank 0)
   invoke them through the SDCC far-call trampoline. Cross-bank calls
   to ui_message() work because ui_message lives in bank 0 (always
   mapped at 0x0000-0x3FFF). */

#define TILE_CHAR(c)  ((uint8_t)(c))
#define SCREEN_W 20
#define SCREEN_H 18

/* ------------------------------------------------------------------ */
/* Local numeric-drawing helpers                                      */
/* ------------------------------------------------------------------ */

static uint8_t ui_draw_u8(uint8_t x, uint8_t y, uint8_t val, uint8_t pal) {
    char buf[4];
    uint8_t len;
    uint8_t v;

    len = 0;
    v = val;
    if (v == 0) {
        buf[0] = '0';
        len = 1;
    } else {
        while (v > 0) {
            buf[len] = '0' + (v % 10);
            v /= 10;
            len++;
        }
        if (len == 2) {
            char t;
            t = buf[0]; buf[0] = buf[1]; buf[1] = t;
        } else if (len == 3) {
            char t;
            t = buf[0]; buf[0] = buf[2]; buf[2] = t;
        }
    }
    buf[len] = '\0';
    ui_draw_text(x, y, buf, pal);
    return len;
}

static uint8_t ui_draw_u16(uint8_t x, uint8_t y, uint16_t val, uint8_t pal) {
    char buf[6];
    uint8_t len;
    uint16_t v;
    uint8_t i, j;
    char t;

    len = 0;
    v = val;
    if (v == 0) {
        buf[0] = '0';
        len = 1;
    } else {
        while (v > 0) {
            buf[len] = '0' + (uint8_t)(v % 10);
            v /= 10;
            len++;
        }
        i = 0;
        j = len - 1;
        while (i < j) {
            t = buf[i]; buf[i] = buf[j]; buf[j] = t;
            i++;
            j--;
        }
    }
    buf[len] = '\0';
    ui_draw_text(x, y, buf, pal);
    return len;
}

static uint8_t ui_draw_s8(uint8_t x, uint8_t y, int8_t val, uint8_t pal) {
    uint8_t off;

    off = 0;
    if (val < 0) {
        ui_draw_text(x, y, "-", pal);
        off = 1;
        val = -val;
    }
    return off + ui_draw_u8(x + off, y, (uint8_t)val, pal);
}

/* ------------------------------------------------------------------ */
/* Action menu                                                        */
/* ------------------------------------------------------------------ */

static const char *const action_labels[] = {
    "Inventory",
    "Eat",
    "Quaff",
    "Read",
    "Zap",
    "Rest",
    "Pick up",
    "Drop",
    "Save+Quit"
};
#define NUM_ACTIONS 9

static const uint8_t action_ids[] = {
    ACTION_INVENTORY,
    ACTION_EAT,
    ACTION_QUAFF,
    ACTION_READ,
    ACTION_ZAP,
    ACTION_WAIT,
    ACTION_PICKUP,
    ACTION_DROP,
    ACTION_SAVE_QUIT
};

static void action_menu_draw(uint8_t cursor) {
    uint8_t i;
    uint8_t ty;

    ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);
    ui_draw_text(5, 1, "= Actions =", PAL_UI);

    for (i = 0; i < NUM_ACTIONS; i++) {
        ty = 3 + i;
        if (i == cursor) {
            ui_draw_text(2, ty, ">", PAL_UI);
        } else {
            ui_draw_text(2, ty, " ", PAL_UI);
        }
        ui_draw_text(3, ty, action_labels[i], PAL_UI);
    }
}

uint8_t ui_action_menu(void) BANKED {
    uint8_t cursor;
    uint8_t result;

    cursor = 0;
    action_menu_draw(cursor);

    for (;;) {
        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP) {
            if (cursor > 0) cursor--;
            else cursor = NUM_ACTIONS - 1;
            action_menu_draw(cursor);
        }

        if (joy_pressed & J_DOWN) {
            if (cursor < NUM_ACTIONS - 1) cursor++;
            else cursor = 0;
            action_menu_draw(cursor);
        }

        if (joy_pressed & J_A) {
            result = action_ids[cursor];
            ui_needs_redraw = 1;
            return result;
        }

        if (joy_pressed & J_B) {
            ui_needs_redraw = 1;
            return ACTION_NONE;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Direction prompt                                                   */
/* ------------------------------------------------------------------ */

uint8_t ui_direction_prompt(void) BANKED {
    ui_message("Direction?");
    ui_show_messages();

    for (;;) {
        wait_vbl_done();
        input_update();

        if (joy_current & J_B) {
            if ((joy_pressed & J_UP) && (joy_current & J_RIGHT))   return DIR_NE;
            if ((joy_pressed & J_UP) && (joy_current & J_LEFT))    return DIR_NW;
            if ((joy_pressed & J_DOWN) && (joy_current & J_RIGHT)) return DIR_SE;
            if ((joy_pressed & J_DOWN) && (joy_current & J_LEFT))  return DIR_SW;
            if ((joy_pressed & J_RIGHT) && (joy_current & J_UP))   return DIR_NE;
            if ((joy_pressed & J_RIGHT) && (joy_current & J_DOWN)) return DIR_SE;
            if ((joy_pressed & J_LEFT) && (joy_current & J_UP))    return DIR_NW;
            if ((joy_pressed & J_LEFT) && (joy_current & J_DOWN))  return DIR_SW;
        }

        if (!(joy_current & J_B)) {
            if (joy_pressed & J_UP)    return DIR_N;
            if (joy_pressed & J_DOWN)  return DIR_S;
            if (joy_pressed & J_LEFT)  return DIR_W;
            if (joy_pressed & J_RIGHT) return DIR_E;
        }

        if ((joy_pressed & J_B) && !(joy_current & (J_UP | J_DOWN | J_LEFT | J_RIGHT))) {
            ui_message("Cancelled.");
            ui_show_messages();
            return DIR_NONE;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Character sheet                                                    */
/* ------------------------------------------------------------------ */

static const char *const hunger_names[] = {
    "Satiated",
    "Normal",
    "Hungry",
    "Weak",
    "Fainting",
    "Starved"
};

void ui_character_sheet(void) BANKED {
    uint8_t row;
    uint8_t cx;
    uint8_t i;

    ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);

    row = 1;
    ui_draw_text(1, row, "= Adventurer =", PAL_UI);
    row += 2;

    ui_draw_text(1, row, "Lv:", PAL_UI);
    ui_draw_u8(5, row, player.level, PAL_UI);
    row++;

    ui_draw_text(1, row, "HP:", PAL_UI);
    cx = 5;
    cx += ui_draw_u8(cx, row, player.hp, PAL_UI);
    ui_draw_text(cx, row, "/", PAL_UI);
    cx++;
    ui_draw_u8(cx, row, player.max_hp, PAL_UI);
    row++;

    ui_draw_text(1, row, "AC:", PAL_UI);
    ui_draw_s8(5, row, player.ac, PAL_UI);
    row++;

    ui_draw_text(1, row, "Str:", PAL_UI);
    ui_draw_u8(5, row, player.strength, PAL_UI);
    row++;

    ui_draw_text(1, row, "XP:", PAL_UI);
    ui_draw_u16(5, row, player.xp, PAL_UI);
    row++;

    ui_draw_text(1, row, "Au:", PAL_UI);
    ui_draw_u16(5, row, player.gold, PAL_UI);
    row++;

    ui_draw_text(1, row, "Dlvl:", PAL_UI);
    ui_draw_u8(7, row, player.dungeon_level, PAL_UI);
    row++;

    ui_draw_text(1, row, "Food:", PAL_UI);
    if (player.hunger_state < 6) {
        ui_draw_text(7, row, hunger_names[player.hunger_state], PAL_UI);
    }
    row++;

    ui_draw_text(1, row, "Turn:", PAL_UI);
    ui_draw_u16(7, row, player.turns, PAL_UI);
    row += 2;

    ui_draw_text(1, row, "- Equipment -", PAL_UI);
    row++;

    for (i = 0; i < MAX_INVENTORY; i++) {
        const char *name;

        if (inventory[i].quantity == 0) continue;
        if (!(inventory[i].flags & IFLAG_EQUIPPED)) continue;
        if (row >= SCREEN_H - 1) break;

        if (inventory[i].flags & IFLAG_IDENTIFIED) {
            name = item_name(inventory[i].type_id);
        } else {
            name = item_appearance_name(inventory[i].type_id);
        }
        ui_draw_text(2, row, name, PAL_UI);
        row++;
    }

    for (;;) {
        wait_vbl_done();
        input_update();
        if (joy_pressed) break;
    }

    ui_needs_redraw = 1;
}

/* ------------------------------------------------------------------ */
/* Message history                                                    */
/* ------------------------------------------------------------------ */

void ui_message_history(void) BANKED {
    uint8_t scroll_top;
    uint8_t max_rows;
    uint8_t i;
    uint8_t idx;
    uint8_t need_draw;

    if (msg_count == 0) {
        ui_message("No messages.");
        ui_show_messages();
        return;
    }

    max_rows = SCREEN_H - 2;
    scroll_top = 0;
    need_draw = 1;

    for (;;) {
        if (need_draw) {
            ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);
            ui_draw_text(3, 0, " Messages ", PAL_UI);

            for (i = 0; i < max_rows; i++) {
                uint8_t msg_idx_offset;

                msg_idx_offset = scroll_top + i;
                if (msg_idx_offset >= msg_count) break;

                if (msg_head >= msg_idx_offset + 1) {
                    idx = msg_head - msg_idx_offset - 1;
                } else {
                    idx = MSG_LOG_SIZE - (msg_idx_offset + 1 - msg_head);
                }

                ui_draw_text(1, 1 + i, msg_log[idx], PAL_UI);
            }
            need_draw = 0;
        }

        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP) {
            if (scroll_top > 0) {
                scroll_top--;
                need_draw = 1;
            }
        }

        if (joy_pressed & J_DOWN) {
            if (scroll_top + max_rows < msg_count) {
                scroll_top++;
                need_draw = 1;
            }
        }

        if ((joy_pressed & J_B) || (joy_pressed & J_SELECT)) break;
        if (joy_pressed & J_A) break;
    }

    ui_needs_redraw = 1;
}

/* ------------------------------------------------------------------ */
/* Help screen                                                        */
/* ------------------------------------------------------------------ */

void ui_help_screen(void) BANKED {
    ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);
    ui_draw_text(5, 0, " Help ", PAL_UI);

    ui_draw_text(1, 1,  "Dpad  Move", PAL_UI);
    ui_draw_text(1, 2,  "A     Action menu", PAL_UI);
    ui_draw_text(1, 3,  "B     Get/Rest", PAL_UI);
    ui_draw_text(1, 4,  "B+Pad Diagonal", PAL_UI);
    ui_draw_text(1, 5,  "Start Character", PAL_UI);
    ui_draw_text(1, 6,  "Sel   This menu", PAL_UI);
    ui_draw_text(1, 8,  "Move to a door", PAL_UI);
    ui_draw_text(1, 9,  " to open it.", PAL_UI);
    ui_draw_text(1, 10, "Move to a monster", PAL_UI);
    ui_draw_text(1, 11, " to attack it.", PAL_UI);
    ui_draw_text(1, 13, "B with no item", PAL_UI);
    ui_draw_text(1, 14, " underfoot: rest.", PAL_UI);

    ui_draw_text(2, 16, "Press any button", PAL_UI);

    for (;;) {
        wait_vbl_done();
        input_update();
        if (joy_pressed) break;
    }

    ui_needs_redraw = 1;
}

/* ------------------------------------------------------------------ */
/* Select menu                                                        */
/* ------------------------------------------------------------------ */

#define SEL_HELP       0
#define SEL_MESSAGES   1
#define SEL_MUSIC      2
#define SEL_SFX        3
#define SEL_QUIT       4
#define SEL_COUNT      5

static void select_menu_draw(uint8_t cursor) {
    uint8_t i;
    uint8_t ty;
    const char *labels[SEL_COUNT];

    labels[0] = "Help";
    labels[1] = "View Messages";
    labels[2] = sound_music_enabled ? "Music: ON" : "Music: OFF";
    labels[3] = sound_sfx_enabled ? "SFX: ON" : "SFX: OFF";
    labels[4] = "Save+Quit";

    ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);
    ui_draw_text(5, 0, " Menu ", PAL_UI);

    for (i = 0; i < SEL_COUNT; i++) {
        ty = 2 + i * 2;
        if (i == cursor) {
            ui_draw_text(2, ty, ">", PAL_UI);
        } else {
            ui_draw_text(2, ty, " ", PAL_UI);
        }
        ui_draw_text(3, ty, labels[i], PAL_UI);
    }
}

uint8_t ui_select_menu(void) BANKED {
    uint8_t cursor;
    uint8_t prev_cursor;
    uint8_t need_draw;
    uint8_t ty;

    cursor = 0;
    prev_cursor = 0;
    need_draw = 1;

    for (;;) {
        if (need_draw) {
            select_menu_draw(cursor);
            need_draw = 0;
        } else if (prev_cursor != cursor) {
            ty = 2 + prev_cursor * 2;
            ui_draw_text(2, ty, " ", PAL_UI);
            ty = 2 + cursor * 2;
            ui_draw_text(2, ty, ">", PAL_UI);
        }
        prev_cursor = cursor;

        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP) {
            if (cursor > 0) cursor--;
            else cursor = SEL_COUNT - 1;
        }

        if (joy_pressed & J_DOWN) {
            if (cursor < SEL_COUNT - 1) cursor++;
            else cursor = 0;
        }

        if (joy_pressed & J_A) {
            switch (cursor) {
            case SEL_HELP:
                ui_help_screen();
                ui_needs_redraw = 1;
                return SEL_HELP;
            case SEL_MESSAGES:
                ui_message_history();
                ui_needs_redraw = 1;
                return SEL_MESSAGES;
            case SEL_MUSIC:
                sound_toggle_music();
                need_draw = 1;
                break;
            case SEL_SFX:
                sound_toggle_sfx();
                need_draw = 1;
                break;
            case SEL_QUIT:
                ui_needs_redraw = 1;
                return SEL_QUIT;
            }
        }

        if ((joy_pressed & J_B) || (joy_pressed & J_SELECT)) {
            ui_needs_redraw = 1;
            return 255;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Yes/No prompt                                                      */
/* ------------------------------------------------------------------ */

uint8_t ui_yes_no(const char *question) BANKED {
    uint8_t cursor;

    cursor = 1;

    ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);
    ui_draw_text(1, 4, question, PAL_UI);

    for (;;) {
        if (cursor == 0) {
            ui_draw_text(4, 8, ">Yes  No", PAL_UI);
        } else {
            ui_draw_text(4, 8, " Yes >No", PAL_UI);
        }

        wait_vbl_done();
        input_update();

        if (joy_pressed & J_LEFT)  cursor = 0;
        if (joy_pressed & J_RIGHT) cursor = 1;

        if (joy_pressed & J_A) {
            ui_needs_redraw = 1;
            return (cursor == 0) ? 1 : 0;
        }

        if (joy_pressed & J_B) {
            ui_needs_redraw = 1;
            return 0;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Pet choice                                                         */
/* ------------------------------------------------------------------ */

uint8_t ui_pet_choice(void) BANKED {
    uint8_t cursor;
    uint8_t prev_cursor;

    cursor = 0;
    prev_cursor = 0;

    ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);
    ui_draw_text(3, 2, "Starting Pet?", PAL_UI);

    ui_draw_text(7, 6, "Cat", PAL_UI);
    ui_draw_text(7, 8, "Dog", PAL_UI);

    ui_draw_text(5, 6, ">", PAL_UI);
    ui_draw_text(5, 8, " ", PAL_UI);

    for (;;) {
        if (prev_cursor != cursor) {
            ui_draw_text(5, 6 + prev_cursor * 2, " ", PAL_UI);
            ui_draw_text(5, 6 + cursor * 2, ">", PAL_UI);
        }
        prev_cursor = cursor;

        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP)   cursor = 0;
        if (joy_pressed & J_DOWN) cursor = 1;

        if ((joy_pressed & J_A) || (joy_pressed & J_START)) {
            ui_needs_redraw = 1;
            return cursor + 1;
        }
    }
}
