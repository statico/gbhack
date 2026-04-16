#include "ui.h"
#include "input.h"
#include "inventory.h"
#include "items.h"
#include "player.h"
#include "render.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

uint8_t ui_needs_redraw;

/* Circular message log */
static char msg_log[MSG_LOG_SIZE][MSG_MAX_LEN + 1];
static uint8_t msg_head;       /* next write position */
static uint8_t msg_count;      /* total stored (max MSG_LOG_SIZE) */
static uint8_t msg_pending;    /* lines pending: 0=none, 1=line1, 2=both lines */
static uint16_t msg_last_turn; /* turn when last message was posted */
static uint8_t msg_displayed;  /* a message is currently on screen */

/* ------------------------------------------------------------------ */
/* Tile helpers                                                       */
/*                                                                    */
/* Tiles 32-127 map 1:1 to ASCII codes.  Box-drawing uses:            */
/*   '+' (43) for corners, '-' (45) for horizontal, '|' (124) for     */
/*   vertical.  PAL_UI (palette 7) is used for all UI text.           */
/* ------------------------------------------------------------------ */

/* Tile index for an ASCII char */
#define TILE_CHAR(c)  ((uint8_t)(c))

/* Screen dimensions (BG map is 32x32, but LCD shows 20x18) */
#define SCREEN_W 20
#define SCREEN_H 18

/* Message line sits at the bottom two rows of the visible screen */
#define MSG_LINE_Y  (SCREEN_H - MSG_ROWS)  /* row 16 */

/* ------------------------------------------------------------------ */
/* Low-level drawing                                                  */
/* ------------------------------------------------------------------ */

void ui_draw_text(uint8_t x, uint8_t y, const char *str, uint8_t pal) {
    uint8_t i;
    uint8_t ch;

    /* Write tile indices */
    VBK_REG = 0;
    for (i = 0; str[i] != '\0'; i++) {
        ch = (uint8_t)str[i];
        set_bkg_tile_xy(x + i, y, ch);
    }

    /* Write palette attributes */
    VBK_REG = 1;
    for (i = 0; str[i] != '\0'; i++) {
        set_bkg_tile_xy(x + i, y, pal);
    }
    VBK_REG = 0;
}

void ui_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pal) {
    uint8_t i;
    uint8_t right;
    uint8_t bottom;

    right = x + w - 1;
    bottom = y + h - 1;

    /* Corners */
    VBK_REG = 0;
    set_bkg_tile_xy(x, y, TILE_CHAR('+'));
    set_bkg_tile_xy(right, y, TILE_CHAR('+'));
    set_bkg_tile_xy(x, bottom, TILE_CHAR('+'));
    set_bkg_tile_xy(right, bottom, TILE_CHAR('+'));

    /* Top and bottom edges */
    for (i = x + 1; i < right; i++) {
        set_bkg_tile_xy(i, y, TILE_CHAR('-'));
        set_bkg_tile_xy(i, bottom, TILE_CHAR('-'));
    }

    /* Left and right edges + interior fill */
    for (i = y + 1; i < bottom; i++) {
        uint8_t j;
        set_bkg_tile_xy(x, i, TILE_CHAR('|'));
        set_bkg_tile_xy(right, i, TILE_CHAR('|'));
        for (j = x + 1; j < right; j++) {
            set_bkg_tile_xy(j, i, TILE_CHAR(' '));
        }
    }

    /* Write palette for the whole box region */
    VBK_REG = 1;
    for (i = y; i <= bottom; i++) {
        uint8_t j;
        for (j = x; j <= right; j++) {
            set_bkg_tile_xy(j, i, pal);
        }
    }
    VBK_REG = 0;
}

/* Fill a rectangular region with spaces */
static void ui_clear_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    uint8_t i, j;

    VBK_REG = 0;
    for (i = y; i < y + h; i++) {
        for (j = x; j < x + w; j++) {
            set_bkg_tile_xy(j, i, TILE_CHAR(' '));
        }
    }
}

/* Write an unsigned 8-bit number at (x,y).  Returns number of digits written. */
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
        /* Build digits in reverse */
        while (v > 0) {
            buf[len] = '0' + (v % 10);
            v /= 10;
            len++;
        }
        /* Reverse */
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

/* Write an unsigned 16-bit number at (x,y).  Returns number of digits written. */
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
        /* Reverse in place */
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

/* Write a signed 8-bit number */
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
/* Message system                                                     */
/* ------------------------------------------------------------------ */

void ui_init(void) {
    uint8_t i;

    msg_head = 0;
    msg_count = 0;
    msg_pending = 0;
    msg_last_turn = 0;
    msg_displayed = 0;
    ui_needs_redraw = 0;

    for (i = 0; i < MSG_LOG_SIZE; i++) {
        msg_log[i][0] = '\0';
    }
}

void ui_message(const char *msg) {
    uint8_t i;

    /* If both message lines are already pending, show "--More--"
       before clearing and starting fresh on line 1. */
    if (msg_pending >= 2) {
        ui_message_more();
    }

    /* If no pending messages, clear any stale display */
    if (msg_pending == 0 && msg_displayed) {
        render_clear_message();
    }

    /* Copy into circular buffer, truncating at MSG_MAX_LEN */
    for (i = 0; i < MSG_MAX_LEN && msg[i] != '\0'; i++) {
        msg_log[msg_head][i] = msg[i];
    }
    msg_log[msg_head][i] = '\0';

    msg_head++;
    if (msg_head >= MSG_LOG_SIZE) {
        msg_head = 0;
    }
    if (msg_count < MSG_LOG_SIZE) {
        msg_count++;
    }

    msg_pending++;
    msg_displayed = 1;
    msg_last_turn = 0;  /* reset so tick counts from next call */
}

void ui_show_messages(void) {
    uint8_t idx;

    if (!msg_pending) {
        return;
    }

    /* Clear message rows */
    ui_clear_rect(0, MSG_LINE_Y, SCREEN_W, MSG_ROWS);

    if (msg_count == 0) {
        msg_pending = 0;
        return;
    }

    if (msg_pending >= 2) {
        /* Two messages pending: show line 1 (second-to-last) and line 2 (last) */
        /* Second-to-last message on line 1 */
        idx = msg_head;
        if (idx <= 1) {
            idx = MSG_LOG_SIZE - (2 - idx);
        } else {
            idx -= 2;
        }
        ui_draw_text(0, MSG_LINE_Y, msg_log[idx], PAL_UI);

        /* Most recent message on line 2 */
        idx = msg_head;
        if (idx == 0) {
            idx = MSG_LOG_SIZE - 1;
        } else {
            idx--;
        }
        ui_draw_text(0, MSG_LINE_Y + 1, msg_log[idx], PAL_UI);
    } else {
        /* Single message: show on line 1 */
        idx = msg_head;
        if (idx == 0) {
            idx = MSG_LOG_SIZE - 1;
        } else {
            idx--;
        }
        ui_draw_text(0, MSG_LINE_Y, msg_log[idx], PAL_UI);
    }

    msg_pending = 0;
}

void ui_message_more(void) {
    /* Show current message + "--More--" and wait for any keypress */
    ui_show_messages();
    ui_draw_text(0, MSG_LINE_Y + 1, "--More--", PAL_UI);

    /* Wait for a new keypress */
    for (;;) {
        wait_vbl_done();
        input_update();
        if (joy_pressed) {
            break;
        }
    }

    /* Clear the more prompt */
    ui_clear_rect(0, MSG_LINE_Y, SCREEN_W, MSG_ROWS);
    msg_pending = 0;
}

/* ------------------------------------------------------------------ */
/* Action menu                                                        */
/* ------------------------------------------------------------------ */

/* Menu labels stored in ROM */
static const char *const action_labels[] = {
    "Inventory",
    "Eat",
    "Quaff",
    "Read",
    "Zap",
    "Search",
    "Pick up",
    "Drop",
    "Wait",
    "Save+Quit"
};
#define NUM_ACTIONS 10

/* Map menu index to ACTION_* id */
static const uint8_t action_ids[] = {
    ACTION_INVENTORY,
    ACTION_EAT,
    ACTION_QUAFF,
    ACTION_READ,
    ACTION_ZAP,
    ACTION_SEARCH,
    ACTION_PICKUP,
    ACTION_DROP,
    ACTION_WAIT,
    ACTION_SAVE_QUIT
};

/* Draw the action menu full-screen */
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

uint8_t ui_action_menu(void) {
    uint8_t cursor;
    uint8_t result;

    cursor = 0;
    action_menu_draw(cursor);

    for (;;) {
        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP) {
            if (cursor > 0) {
                cursor--;
            } else {
                cursor = NUM_ACTIONS - 1;
            }
            action_menu_draw(cursor);
        }

        if (joy_pressed & J_DOWN) {
            if (cursor < NUM_ACTIONS - 1) {
                cursor++;
            } else {
                cursor = 0;
            }
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
/* Inventory screen                                                   */
/* ------------------------------------------------------------------ */

uint8_t ui_inventory_screen(uint8_t filter_category) {
    uint8_t slot_map[MAX_INVENTORY]; /* maps visible row -> inventory slot */
    uint8_t visible_count;
    uint8_t cursor;
    uint8_t scroll_top;
    uint8_t i;
    uint8_t max_visible;
    const ItemType *itype;

    /* Build list of matching slots */
    visible_count = 0;
    for (i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].type_id == 0 && inventory[i].quantity == 0) {
            continue;
        }
        if (inventory[i].quantity == 0) {
            continue;
        }
        if (filter_category != 255) {
            itype = &item_types[inventory[i].type_id];
            if (itype->category != filter_category) {
                continue;
            }
        }
        slot_map[visible_count] = i;
        visible_count++;
    }

    if (visible_count == 0) {
        ui_message("Nothing to show.");
        ui_show_messages();
        ui_needs_redraw = 1;
        return 255;
    }

    /* Maximum rows we can display (screen height minus border rows) */
    max_visible = SCREEN_H - 2;  /* 16 rows */
    if (max_visible > visible_count) {
        max_visible = visible_count;
    }

    cursor = 0;
    scroll_top = 0;

    {
    uint8_t full_redraw;
    uint8_t prev_cursor;
    full_redraw = 1;
    prev_cursor = cursor;

    for (;;) {
        uint8_t row;
        uint8_t slot;
        uint8_t letter;
        char line[SCREEN_W + 1];
        uint8_t lp;
        const char *name;

        if (full_redraw) {
        /* Clear screen and draw border */
        ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);

        /* Draw visible items */
        for (row = 0; row < max_visible; row++) {
            uint8_t idx;

            idx = scroll_top + row;
            if (idx >= visible_count) {
                break;
            }
            slot = slot_map[idx];
            letter = 'a' + slot;

            /* Build the display line: "a) Item name" */
            lp = 0;
            line[lp++] = (char)letter;
            line[lp++] = ')';
            line[lp++] = ' ';

            /* Choose name based on identification status */
            itype = &item_types[inventory[slot].type_id];
            if (inventory[slot].flags & IFLAG_IDENTIFIED) {
                name = item_name(inventory[slot].type_id);
            } else {
                name = item_appearance_name(inventory[slot].type_id);
            }

            /* Copy name, leave room for suffix */
            for (i = 0; name[i] != '\0' && lp < SCREEN_W - 5; i++) {
                line[lp++] = name[i];
            }

            /* Equipped marker */
            if (inventory[slot].flags & IFLAG_EQUIPPED) {
                line[lp++] = ' ';
                line[lp++] = '[';
                line[lp++] = 'E';
                line[lp++] = ']';
            }

            line[lp] = '\0';

            /* Draw cursor or space, then the line */
            if (idx == cursor) {
                ui_draw_text(1, 1 + row, ">", PAL_UI);
            } else {
                ui_draw_text(1, 1 + row, " ", PAL_UI);
            }
            ui_draw_text(2, 1 + row, line, PAL_UI);
        }

        full_redraw = 0;
        } else if (prev_cursor != cursor) {
            /* Only update the two affected cursor rows, no full redraw */
            uint8_t old_row, new_row;
            old_row = prev_cursor - scroll_top;
            new_row = cursor - scroll_top;
            if (old_row < max_visible) {
                ui_draw_text(1, 1 + old_row, " ", PAL_UI);
            }
            if (new_row < max_visible) {
                ui_draw_text(1, 1 + new_row, ">", PAL_UI);
            }
        }

        prev_cursor = cursor;

        /* Input loop */
        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP) {
            if (cursor > 0) {
                cursor--;
                if (cursor < scroll_top) {
                    scroll_top = cursor;
                    full_redraw = 1;
                }
            }
        }

        if (joy_pressed & J_DOWN) {
            if (cursor < visible_count - 1) {
                cursor++;
                if (cursor >= scroll_top + max_visible) {
                    scroll_top = cursor - max_visible + 1;
                    full_redraw = 1;
                }
            }
        }

        if (joy_pressed & J_A) {
            ui_needs_redraw = 1;
            return slot_map[cursor];
        }

        if (joy_pressed & J_B) {
            ui_needs_redraw = 1;
            return 255;
        }
    }
    } /* end cursor block */
}

/* ------------------------------------------------------------------ */
/* Direction prompt                                                   */
/* ------------------------------------------------------------------ */

uint8_t ui_direction_prompt(void) {
    ui_message("Direction?");
    ui_show_messages();

    for (;;) {
        wait_vbl_done();
        input_update();

        /* Check diagonals first: B held + d-pad press */
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

        /* Cardinal directions (only when B is not held) */
        if (!(joy_current & J_B)) {
            if (joy_pressed & J_UP)    return DIR_N;
            if (joy_pressed & J_DOWN)  return DIR_S;
            if (joy_pressed & J_LEFT)  return DIR_W;
            if (joy_pressed & J_RIGHT) return DIR_E;
        }

        /* B alone (pressed without d-pad) cancels */
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

void ui_character_sheet(void) {
    uint8_t row;
    uint8_t cx;
    uint8_t i;

    /* Clear and draw border */
    ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);

    row = 1;

    /* Title */
    ui_draw_text(1, row, "= Adventurer =", PAL_UI);
    row += 2;

    /* Level */
    ui_draw_text(1, row, "Lv:", PAL_UI);
    ui_draw_u8(5, row, player.level, PAL_UI);
    row++;

    /* HP */
    ui_draw_text(1, row, "HP:", PAL_UI);
    cx = 5;
    cx += ui_draw_u8(cx, row, player.hp, PAL_UI);
    ui_draw_text(cx, row, "/", PAL_UI);
    cx++;
    ui_draw_u8(cx, row, player.max_hp, PAL_UI);
    row++;

    /* AC */
    ui_draw_text(1, row, "AC:", PAL_UI);
    ui_draw_s8(5, row, player.ac, PAL_UI);
    row++;

    /* Str */
    ui_draw_text(1, row, "Str:", PAL_UI);
    ui_draw_u8(5, row, player.strength, PAL_UI);
    row++;

    /* XP */
    ui_draw_text(1, row, "XP:", PAL_UI);
    ui_draw_u16(5, row, player.xp, PAL_UI);
    row++;

    /* Gold */
    ui_draw_text(1, row, "Au:", PAL_UI);
    ui_draw_u16(5, row, player.gold, PAL_UI);
    row++;

    /* Dungeon level */
    ui_draw_text(1, row, "Dlvl:", PAL_UI);
    ui_draw_u8(7, row, player.dungeon_level, PAL_UI);
    row++;

    /* Hunger */
    ui_draw_text(1, row, "Food:", PAL_UI);
    if (player.hunger_state < 6) {
        ui_draw_text(7, row, hunger_names[player.hunger_state], PAL_UI);
    }
    row++;

    /* Turns */
    ui_draw_text(1, row, "Turn:", PAL_UI);
    ui_draw_u16(7, row, player.turns, PAL_UI);
    row += 2;

    /* Equipped items */
    ui_draw_text(1, row, "- Equipment -", PAL_UI);
    row++;

    for (i = 0; i < MAX_INVENTORY; i++) {
        const char *name;

        if (inventory[i].quantity == 0) {
            continue;
        }
        if (!(inventory[i].flags & IFLAG_EQUIPPED)) {
            continue;
        }
        if (row >= SCREEN_H - 1) {
            break;
        }

        if (inventory[i].flags & IFLAG_IDENTIFIED) {
            name = item_name(inventory[i].type_id);
        } else {
            name = item_appearance_name(inventory[i].type_id);
        }
        ui_draw_text(2, row, name, PAL_UI);
        row++;
    }

    /* Wait for any key */
    for (;;) {
        wait_vbl_done();
        input_update();
        if (joy_pressed) {
            break;
        }
    }

    ui_needs_redraw = 1;
}

/* ------------------------------------------------------------------ */
/* Message history                                                    */
/* ------------------------------------------------------------------ */

void ui_message_history(void) {
    uint8_t scroll_top;
    uint8_t max_rows;
    uint8_t i;
    uint8_t idx;

    if (msg_count == 0) {
        ui_message("No messages.");
        ui_show_messages();
        return;
    }

    max_rows = SCREEN_H - 2;  /* rows available inside border */
    scroll_top = 0;

    for (;;) {
        /* Draw border */
        ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);

        /* Draw messages from newest to oldest */
        for (i = 0; i < max_rows; i++) {
            uint8_t msg_idx_offset;

            msg_idx_offset = scroll_top + i;
            if (msg_idx_offset >= msg_count) {
                break;
            }

            /* Calculate actual index into circular buffer.
               Index 0 = most recent message. */
            if (msg_head >= msg_idx_offset + 1) {
                idx = msg_head - msg_idx_offset - 1;
            } else {
                idx = MSG_LOG_SIZE - (msg_idx_offset + 1 - msg_head);
            }

            ui_draw_text(1, 1 + i, msg_log[idx], PAL_UI);
        }

        /* Input */
        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP) {
            if (scroll_top > 0) {
                scroll_top--;
            }
        }

        if (joy_pressed & J_DOWN) {
            if (scroll_top + max_rows < msg_count) {
                scroll_top++;
            }
        }

        if (joy_pressed & J_B) {
            break;
        }

        if (joy_pressed & J_A) {
            break;
        }
    }

    ui_needs_redraw = 1;
}

/* ------------------------------------------------------------------ */
/* Yes/No prompt                                                      */
/* ------------------------------------------------------------------ */

void ui_message_tick(uint16_t current_turn) {
    if (!msg_displayed) return;

    if (msg_last_turn == 0) {
        /* First tick after a message was posted */
        msg_last_turn = current_turn;
        return;
    }

    if (current_turn >= msg_last_turn + 3) {
        render_clear_message();
        msg_displayed = 0;
        msg_last_turn = 0;
    }
}

uint8_t ui_yes_no(const char *question) {
    uint8_t cursor;  /* 0 = Yes, 1 = No */

    cursor = 1;  /* Default to No */

    /* Draw full-screen prompt */
    ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);
    ui_draw_text(1, 4, question, PAL_UI);

    for (;;) {
        /* Draw Yes/No options */
        if (cursor == 0) {
            ui_draw_text(4, 8, ">Yes  No", PAL_UI);
        } else {
            ui_draw_text(4, 8, " Yes >No", PAL_UI);
        }

        wait_vbl_done();
        input_update();

        if (joy_pressed & J_LEFT) {
            cursor = 0;
        }
        if (joy_pressed & J_RIGHT) {
            cursor = 1;
        }

        if (joy_pressed & J_A) {
            ui_needs_redraw = 1;
            return (cursor == 0) ? 1 : 0;
        }

        if (joy_pressed & J_B) {
            ui_needs_redraw = 1;
            return 0;  /* B = No */
        }
    }
}
