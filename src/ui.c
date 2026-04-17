#include "ui.h"
#include "ui_internal.h"
#include "input.h"
#include "inventory.h"
#include "items.h"
#include "player.h"
#include "render.h"
#include "sound.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

uint8_t ui_needs_redraw;

/* Circular message log — non-static so ui_menus.c (bank 7) can render history */
char msg_log[MSG_LOG_SIZE][MSG_MAX_LEN + 1];
uint8_t msg_head;              /* next write position */
uint8_t msg_count;              /* total stored (max MSG_LOG_SIZE) */
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

#define TILE_CHAR(c)  ((uint8_t)(c))

#define SCREEN_W 20
#define SCREEN_H 18

#define MSG_LINE_Y  (SCREEN_H - MSG_ROWS)

/* ------------------------------------------------------------------ */
/* Low-level drawing                                                  */
/* ------------------------------------------------------------------ */

void ui_draw_text(uint8_t x, uint8_t y, const char *str, uint8_t pal) {
    uint8_t i;
    uint8_t ch;

    VBK_REG = 0;
    for (i = 0; str[i] != '\0'; i++) {
        ch = (uint8_t)str[i];
        set_bkg_tile_xy(x + i, y, ch);
    }

    VBK_REG = 1;
    for (i = 0; str[i] != '\0'; i++) {
        set_bkg_tile_xy(x + i, y, pal);
    }
    VBK_REG = 0;
}

#define TILE_BOX_TL  15
#define TILE_BOX_TR  16
#define TILE_BOX_BL  17
#define TILE_BOX_BR  18
#define TILE_BOX_H   19
#define TILE_BOX_V   20

void ui_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pal) {
    uint8_t i;
    uint8_t right;
    uint8_t bottom;

    right = x + w - 1;
    bottom = y + h - 1;

    VBK_REG = 0;
    set_bkg_tile_xy(x, y, TILE_BOX_TL);
    set_bkg_tile_xy(right, y, TILE_BOX_TR);
    set_bkg_tile_xy(x, bottom, TILE_BOX_BL);
    set_bkg_tile_xy(right, bottom, TILE_BOX_BR);

    for (i = x + 1; i < right; i++) {
        set_bkg_tile_xy(i, y, TILE_BOX_H);
        set_bkg_tile_xy(i, bottom, TILE_BOX_H);
    }

    for (i = y + 1; i < bottom; i++) {
        uint8_t j;
        set_bkg_tile_xy(x, i, TILE_BOX_V);
        set_bkg_tile_xy(right, i, TILE_BOX_V);
        for (j = x + 1; j < right; j++) {
            set_bkg_tile_xy(j, i, TILE_CHAR(' '));
        }
    }

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

    if (msg_pending >= 2) {
        ui_message_more();
    }

    if (msg_pending == 0 && msg_displayed) {
        render_clear_message();
    }

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
    msg_last_turn = 0;
}

void ui_show_messages(void) {
    uint8_t idx;

    if (!msg_pending) {
        return;
    }

    ui_clear_rect(0, MSG_LINE_Y, SCREEN_W, MSG_ROWS);

    if (msg_count == 0) {
        msg_pending = 0;
        return;
    }

    if (msg_pending >= 2) {
        idx = msg_head;
        if (idx <= 1) {
            idx = MSG_LOG_SIZE - (2 - idx);
        } else {
            idx -= 2;
        }
        ui_draw_text(0, MSG_LINE_Y, msg_log[idx], PAL_UI);

        idx = msg_head;
        if (idx == 0) {
            idx = MSG_LOG_SIZE - 1;
        } else {
            idx--;
        }
        ui_draw_text(0, MSG_LINE_Y + 1, msg_log[idx], PAL_UI);
    } else {
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
    ui_show_messages();
    ui_draw_text(0, MSG_LINE_Y + 1, "--More--", PAL_UI);

    for (;;) {
        wait_vbl_done();
        input_update();
        if (joy_pressed) {
            break;
        }
    }

    ui_clear_rect(0, MSG_LINE_Y, SCREEN_W, MSG_ROWS);
    msg_pending = 0;
}

/* ------------------------------------------------------------------ */
/* Inventory screen (bank 0 — called from inventory.c bank 5)         */
/* ------------------------------------------------------------------ */

uint8_t ui_inventory_screen(uint8_t filter_category) {
    uint8_t slot_map[MAX_INVENTORY];
    uint8_t visible_count;
    uint8_t cursor;
    uint8_t scroll_top;
    uint8_t i;
    uint8_t max_visible;
    const ItemType *itype;

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

    max_visible = SCREEN_H - 2;
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
        ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);

        for (row = 0; row < max_visible; row++) {
            uint8_t idx;

            idx = scroll_top + row;
            if (idx >= visible_count) {
                break;
            }
            slot = slot_map[idx];
            letter = 'a' + slot;

            lp = 0;
            line[lp++] = (char)letter;
            line[lp++] = ')';
            line[lp++] = ' ';

            itype = &item_types[inventory[slot].type_id];
            if (inventory[slot].flags & IFLAG_IDENTIFIED) {
                name = item_name(inventory[slot].type_id);
            } else {
                name = item_appearance_name(inventory[slot].type_id);
            }

            for (i = 0; name[i] != '\0' && lp < SCREEN_W - 2; i++) {
                line[lp++] = name[i];
            }

            if (itype->category == ICAT_POTION &&
                (inventory[slot].flags & IFLAG_IDENTIFIED) &&
                lp + 5 <= SCREEN_W - 2) {
                line[lp++] = ' '; line[lp++] = 'p'; line[lp++] = 'o';
                line[lp++] = 't'; line[lp++] = '.';
            } else if (itype->category == ICAT_SCROLL &&
                       lp + 5 <= SCREEN_W - 2) {
                line[lp++] = ' '; line[lp++] = 's'; line[lp++] = 'c';
                line[lp++] = 'r'; line[lp++] = 'l';
            }

            if (inventory[slot].flags & IFLAG_EQUIPPED) {
                if (lp + 4 <= SCREEN_W) {
                    line[lp++] = ' ';
                    line[lp++] = '[';
                    line[lp++] = 'E';
                    line[lp++] = ']';
                }
            }

            line[lp] = '\0';

            if (idx == cursor) {
                ui_draw_text(1, 1 + row, ">", PAL_UI);
            } else {
                ui_draw_text(1, 1 + row, " ", PAL_UI);
            }
            ui_draw_text(2, 1 + row, line, PAL_UI);
        }

        full_redraw = 0;
        } else if (prev_cursor != cursor) {
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

        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP) {
            if (cursor > 0) {
                cursor--;
                sound_play_sfx(SFX_MENU_MOVE);
                if (cursor < scroll_top) {
                    scroll_top = cursor;
                    full_redraw = 1;
                }
            }
        }

        if (joy_pressed & J_DOWN) {
            if (cursor < visible_count - 1) {
                cursor++;
                sound_play_sfx(SFX_MENU_MOVE);
                if (cursor >= scroll_top + max_visible) {
                    scroll_top = cursor - max_visible + 1;
                    full_redraw = 1;
                }
            }
        }

        if (joy_pressed & J_A) {
            sound_play_sfx(SFX_MENU_CONFIRM);
            ui_needs_redraw = 1;
            return slot_map[cursor];
        }

        if (joy_pressed & J_B) {
            ui_needs_redraw = 1;
            return 255;
        }
    }
    }
}

/* ------------------------------------------------------------------ */
/* Message staleness tick                                             */
/* ------------------------------------------------------------------ */

void ui_message_tick(uint16_t current_turn) {
    if (!msg_displayed) return;

    if (msg_last_turn == 0) {
        msg_last_turn = current_turn;
        return;
    }

    if (current_turn >= msg_last_turn + 3) {
        render_clear_message();
        msg_displayed = 0;
        msg_last_turn = 0;
    }
}
