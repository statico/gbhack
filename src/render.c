#include "render.h"
#include "dungeon.h"
#include "player.h"
#include "monsters.h"
#include "items.h"
#include "fov.h"
#include "pet.h"
#include "../res/tiles.h"
#include <gb/cgb.h>

/* ---- Tile indices ---- */
#define TILE_BLANK        0
#define TILE_WALL         1
#define TILE_FLOOR        2
#define TILE_CORRIDOR     2   /* Same as floor */
#define TILE_DOOR_CLOSED  3
#define TILE_DOOR_OPEN    4
#define TILE_STAIRS_UP    5
#define TILE_STAIRS_DOWN  6
#define TILE_ALTAR        7
#define TILE_PLAYER       8
#define TILE_FOUNTAIN     9
#define TILE_TRAP        '^'  /* ASCII 94 - standard roguelike trap symbol */
#define TILE_SHOP_FLOOR  11
#define TILE_ICON_HEART     10
#define TILE_ICON_SHIELD    11
#define TILE_ICON_STAIRS    12
#define TILE_ICON_HOURGLASS 13
#define TILE_ICON_SWORD     14
#define TILE_MONSTER_BASE 64
#define TILE_ITEM_BASE   128

/* ---- Camera ---- */
uint8_t camera_x, camera_y;

/* Dirty flag for full redraw */
static uint8_t needs_full_redraw;

/* Message buffer: 2 lines of 20 chars */
static char msg_buf[41]; /* 20 + 20 + null */
static uint8_t msg_active;

/* Row buffer for bulk tile/attribute writes */
static uint8_t row_tiles[VIEWPORT_W];
static uint8_t row_attrs[VIEWPORT_W];

/* ---- Palette data ---- */
/* 8 palettes x 4 colors = 32 entries */
static const uint16_t bg_palettes[] = {
    /* PAL 0 - Terrain: dark stone, warm brown, sandy tan, cream */
    RGB( 6,  5,  8), RGB(14, 11,  8), RGB(22, 18, 12), RGB(28, 25, 20),
    /* PAL 1 - Player: black, white, bright cyan, yellow */
    RGB( 0,  0,  0), RGB(31, 31, 31), RGB( 4, 28, 28), RGB(31, 28,  4),
    /* PAL 2 - Hostile: dark maroon, crimson, orange-red, bright scarlet */
    RGB( 4,  1,  1), RGB(18,  2,  2), RGB(26,  8,  4), RGB(31, 14,  8),
    /* PAL 3 - Neutral: dark olive, olive, warm yellow, bright amber */
    RGB( 4,  4,  1), RGB(12, 14,  4), RGB(24, 22,  6), RGB(31, 28, 12),
    /* PAL 4 - Equipment: dark navy, steel blue, sky blue, bright ice */
    RGB( 2,  2,  6), RGB( 6,  8, 18), RGB(10, 14, 26), RGB(18, 22, 31),
    /* PAL 5 - Consumable: dark forest, emerald, lime, bright green */
    RGB( 2,  4,  2), RGB( 4, 16,  6), RGB( 8, 26, 10), RGB(16, 31, 16),
    /* PAL 6 - Special: dark indigo, amethyst, gold, bright gold */
    RGB( 4,  2,  8), RGB(16,  6, 20), RGB(28, 22,  4), RGB(31, 28, 12),
    /* PAL 7 - UI: near-black, dark gray, light gray, white */
    RGB( 2,  2,  2), RGB(12, 12, 12), RGB(22, 22, 22), RGB(31, 31, 31)
};

/* ---- Terrain tile lookup ---- */
static uint8_t terrain_to_tile(uint8_t terrain) {
    switch (terrain) {
        case TERRAIN_WALL:        return TILE_WALL;
        case TERRAIN_FLOOR:       return TILE_FLOOR;
        case TERRAIN_CORRIDOR:    return TILE_CORRIDOR;
        case TERRAIN_DOOR_CLOSED: return TILE_DOOR_CLOSED;
        case TERRAIN_DOOR_OPEN:   return TILE_DOOR_OPEN;
        case TERRAIN_STAIRS_UP:   return TILE_STAIRS_UP;
        case TERRAIN_STAIRS_DOWN: return TILE_STAIRS_DOWN;
        case TERRAIN_ALTAR:       return TILE_ALTAR;
        case TERRAIN_FOUNTAIN:    return TILE_FOUNTAIN;
        case TERRAIN_TRAP:        return TILE_TRAP;
        case TERRAIN_SHOP_FLOOR:  return TILE_SHOP_FLOOR;
        default:                  return TILE_BLANK;
    }
}

/* Palette for terrain features */
static uint8_t terrain_to_palette(uint8_t terrain) {
    switch (terrain) {
        case TERRAIN_ALTAR:       return PAL_SPECIAL;
        case TERRAIN_FOUNTAIN:    return PAL_SPECIAL;
        case TERRAIN_STAIRS_UP:   return PAL_SPECIAL;
        case TERRAIN_STAIRS_DOWN: return PAL_SPECIAL;
        case TERRAIN_SHOP_FLOOR:  return PAL_SPECIAL;
        case TERRAIN_DOOR_CLOSED: return PAL_NEUTRAL;
        case TERRAIN_DOOR_OPEN:   return PAL_NEUTRAL;
        case TERRAIN_TRAP:        return PAL_HOSTILE;
        default:                  return PAL_TERRAIN;
    }
}

/* Palette for an item by its category */
static uint8_t item_palette(uint8_t type_id) {
    uint8_t cat;
    cat = item_types[type_id].category;
    switch (cat) {
        case ICAT_WEAPON:  return PAL_EQUIPMENT;
        case ICAT_ARMOR:   return PAL_EQUIPMENT;
        case ICAT_POTION:  return PAL_CONSUMABLE;
        case ICAT_SCROLL:  return PAL_CONSUMABLE;
        case ICAT_WAND:    return PAL_EQUIPMENT;
        case ICAT_FOOD:    return PAL_CONSUMABLE;
        case ICAT_GOLD:    return PAL_SPECIAL;
        case ICAT_TOOL:    return PAL_EQUIPMENT;
        case ICAT_AMULET:  return PAL_SPECIAL;
        default:           return PAL_TERRAIN;
    }
}

/* Palette for a monster based on its status */
static uint8_t monster_palette(uint8_t idx) {
    if (monsters[idx].status & MSTAT_PEACEFUL) {
        return PAL_NEUTRAL;
    }
    return PAL_HOSTILE;
}

/* ---- Camera positioning ---- */
static void camera_center_on_player(void) {
    int8_t cx, cy;

    /* Compute desired camera position, centering player in viewport */
    cx = (int8_t)player.x - (VIEWPORT_W / 2);
    cy = (int8_t)player.y - (VIEWPORT_H / 2);

    /* Clamp to map bounds */
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if ((uint8_t)cx > MAP_WIDTH - VIEWPORT_W)
        cx = MAP_WIDTH - VIEWPORT_W;
    if ((uint8_t)cy > MAP_HEIGHT - VIEWPORT_H)
        cy = MAP_HEIGHT - VIEWPORT_H;

    camera_x = (uint8_t)cx;
    camera_y = (uint8_t)cy;
}

/* ---- Determine what tile and palette to draw at a map position ---- */
static void resolve_cell(uint8_t mx, uint8_t my, uint8_t *out_tile, uint8_t *out_pal) {
    uint8_t cell;
    uint8_t terrain;
    uint8_t visible;
    uint8_t midx;
    uint8_t iidx;

    cell = dungeon_get_cell(mx, my);
    terrain = CELL_TERRAIN(cell);
    visible = fov_is_visible(mx, my);

    /* Not seen at all: draw blank */
    if (!(cell & CELL_SEEN) && !visible) {
        *out_tile = TILE_BLANK;
        *out_pal = PAL_TERRAIN;
        return;
    }

    /* Currently visible: show everything */
    if (visible) {
        /* Priority: player > monster > item > terrain */
        if (mx == player.x && my == player.y) {
            *out_tile = TILE_PLAYER;
            *out_pal = PAL_PLAYER;
            return;
        }

        /* Monster */
        if (cell & CELL_HAS_MONSTER) {
            midx = monster_at(mx, my);
            if (midx != 255) {
                /* Pet gets a distinct tile: 'f' for cat, 'd' for dog */
                if (midx == pet_index) {
                    *out_tile = (player.pet_type == 1) ? 'f' : 'd';
                    *out_pal = PAL_CONSUMABLE;
                } else {
                    *out_tile = TILE_MONSTER_BASE + monsters[midx].type_id;
                    *out_pal = monster_palette(midx);
                }
                return;
            }
        }

        /* Item */
        if (cell & CELL_HAS_ITEM) {
            iidx = item_at(mx, my);
            if (iidx != 255) {
                *out_tile = TILE_ITEM_BASE + floor_items[iidx].type_id;
                *out_pal = item_palette(floor_items[iidx].type_id);
                return;
            }
        }

        /* Terrain - full color */
        *out_tile = terrain_to_tile(terrain);
        *out_pal = terrain_to_palette(terrain);
        return;
    }

    /* Seen but not currently visible: show terrain only, muted (PAL 0) */
    *out_tile = terrain_to_tile(terrain);
    *out_pal = PAL_TERRAIN;
}

/* ---- Write a single tile + attribute to BG map ---- */
static void write_bg_tile(uint8_t bx, uint8_t by, uint8_t tile, uint8_t attr) {
    /* Write tile data (VRAM bank 0) */
    VBK_REG = 0;
    set_bkg_tile_xy(bx, by, tile);

    /* Write attribute (VRAM bank 1) */
    VBK_REG = 1;
    set_bkg_tile_xy(bx, by, attr);
    VBK_REG = 0;
}

/* ---- Public API ---- */

void render_init(void) BANKED {
    uint8_t x, y;

    /* Must turn display off before bulk VRAM writes */
    DISPLAY_OFF;

    /* Load tile graphics into VRAM */
    tiles_load();

    /* Set all 8 BG palettes */
    set_bkg_palette(0, 8, bg_palettes);

    /* Clear the entire BG map to blank tiles */
    VBK_REG = 0;
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            set_bkg_tile_xy(x, y, TILE_BLANK);
        }
    }
    VBK_REG = 1;
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            set_bkg_tile_xy(x, y, PAL_TERRAIN);
        }
    }
    VBK_REG = 0;

    camera_x = 0;
    camera_y = 0;
    needs_full_redraw = 1;
    msg_active = 0;
    msg_buf[0] = 0;

    /* Enable background only — window layer at Y=0 would cover the
       entire screen (GBC window extends from WY to bottom), so we
       draw everything (status bar, viewport, messages) on BKG. */
    HIDE_WIN;
    SHOW_BKG;
    LCDC_REG |= LCDCF_BG8000;
    DISPLAY_ON;
}

void render_full_redraw(void) BANKED {
    needs_full_redraw = 1;
}

void render_update(void) BANKED {
    uint8_t vx, vy;
    uint8_t mx, my;
    uint8_t by;
    uint8_t tile, pal;

    camera_center_on_player();

    /* Scroll the BG layer so that the viewport starts below the window.
     * The window occupies the top STATUS_ROWS * 8 pixels.
     * BG scroll positions: we write tiles at BG positions 0..19, 0..15
     * and use SCX/SCY = 0 since we write directly into the visible area.
     * Actually, we write to wrapped positions and let HW scroll handle it. */
    SCX_REG = 0;
    SCY_REG = 0;

    wait_vbl_done();

    for (vy = 0; vy < VIEWPORT_H; vy++) {
        my = camera_y + vy;

        /* Build row of tiles and attributes */
        for (vx = 0; vx < VIEWPORT_W; vx++) {
            mx = camera_x + vx;
            resolve_cell(mx, my, &tile, &pal);
            row_tiles[vx] = tile;
            row_attrs[vx] = pal;
        }

        /* BG row position: offset by STATUS_ROWS to leave room for window overlay.
         * Wrap within the 32-row BG map. */
        by = (vy + STATUS_ROWS) & 31;

        /* Write tile row */
        VBK_REG = 0;
        set_bkg_tiles(0, by, VIEWPORT_W, 1, row_tiles);

        /* Write attribute row */
        VBK_REG = 1;
        set_bkg_tiles(0, by, VIEWPORT_W, 1, row_attrs);
        VBK_REG = 0;
    }

    /* Render message rows at the bottom of the viewport */
    if (msg_active) {
        uint8_t mi, mby, mlen;
        char mline[21];

        mlen = 0;
        while (msg_buf[mlen] != 0 && mlen < 40) mlen++;

        /* Line 1: first 20 chars */
        for (mi = 0; mi < 20; mi++) {
            mline[mi] = (mi < mlen) ? msg_buf[mi] : ' ';
        }
        mby = (VIEWPORT_H + STATUS_ROWS) & 31;
        VBK_REG = 0;
        for (mi = 0; mi < 20; mi++) {
            set_bkg_tile_xy(mi, mby, (uint8_t)mline[mi]);
        }
        VBK_REG = 1;
        for (mi = 0; mi < 20; mi++) {
            set_bkg_tile_xy(mi, mby, PAL_UI);
        }

        /* Line 2: chars 20..39 */
        for (mi = 0; mi < 20; mi++) {
            mline[mi] = (mi + 20 < mlen) ? msg_buf[mi + 20] : ' ';
        }
        mby = (VIEWPORT_H + STATUS_ROWS + 1) & 31;
        VBK_REG = 0;
        for (mi = 0; mi < 20; mi++) {
            set_bkg_tile_xy(mi, mby, (uint8_t)mline[mi]);
        }
        VBK_REG = 1;
        for (mi = 0; mi < 20; mi++) {
            set_bkg_tile_xy(mi, mby, PAL_UI);
        }
        VBK_REG = 0;
    }

    needs_full_redraw = 0;
}

void render_tile_at(uint8_t map_x, uint8_t map_y) BANKED {
    uint8_t tile, pal;
    uint8_t bx, by;
    int8_t vx_s, vy_s;

    /* Check if tile is within current viewport */
    vx_s = (int8_t)(map_x - camera_x);
    vy_s = (int8_t)(map_y - camera_y);
    if (vx_s < 0 || vx_s >= VIEWPORT_W) return;
    if (vy_s < 0 || vy_s >= VIEWPORT_H) return;

    resolve_cell(map_x, map_y, &tile, &pal);

    bx = (uint8_t)vx_s;
    by = ((uint8_t)vy_s + STATUS_ROWS) & 31;

    write_bg_tile(bx, by, tile, pal);
}

/* ---- Status bar ---- */

/* Helper: write a decimal number into a buffer.
 * Returns number of chars written. */
static uint8_t write_num(char *buf, uint16_t val, uint8_t max_digits) {
    uint8_t len;
    uint8_t i;
    char tmp[6];

    if (val == 0) {
        buf[0] = '0';
        return 1;
    }
    len = 0;
    while (val > 0 && len < max_digits) {
        tmp[len] = '0' + (uint8_t)(val % 10);
        val /= 10;
        len++;
    }
    /* Reverse */
    for (i = 0; i < len; i++) {
        buf[i] = tmp[len - 1 - i];
    }
    return len;
}

void render_status_bar(void) BANKED {
    uint8_t tiles[20];
    uint8_t attrs[20];
    uint8_t pos;
    uint8_t i;
    uint8_t hp_start, hp_end;
    uint8_t ac_start, ac_end;
    uint8_t lv_start, lv_end;
    uint8_t gold_start, gold_end;
    uint8_t turn_start, turn_end;
    uint8_t hp_pal;

    /* Format: heart HP  armor AC  Lv#  $gold  T turns */
    for (i = 0; i < 20; i++) {
        tiles[i] = ' ';
        attrs[i] = PAL_UI;
    }

    pos = 0;

    /* Heart icon + HP (up to 3 digits) */
    hp_start = pos;
    tiles[pos++] = TILE_ICON_HEART;
    pos += write_num((char *)&tiles[pos], player.hp, 3);
    hp_end = pos;
    tiles[pos++] = ' ';

    /* Use armor item tile for AC icon */
    ac_start = pos;
    tiles[pos++] = (uint8_t)(TILE_ITEM_BASE + 7);  /* chain mail tile */
    if (player.ac < 0) {
        tiles[pos++] = '-';
        pos += write_num((char *)&tiles[pos], (uint16_t)(-(int16_t)player.ac), 2);
    } else {
        pos += write_num((char *)&tiles[pos], (uint16_t)player.ac, 2);
    }
    ac_end = pos;
    tiles[pos++] = ' ';

    /* Level */
    lv_start = pos;
    tiles[pos++] = 'L';
    pos += write_num((char *)&tiles[pos], player.level, 2);
    lv_end = pos;
    tiles[pos++] = ' ';

    /* Gold ($ + up to 5 digits) */
    gold_start = pos;
    tiles[pos++] = '$';
    pos += write_num((char *)&tiles[pos], player.gold, 5);
    gold_end = pos;
    tiles[pos++] = ' ';

    /* Turn count (T + up to 5 digits, only if room) */
    if (pos < 19) {
        turn_start = pos;
        tiles[pos++] = 'T';
        pos += write_num((char *)&tiles[pos], player.turns, 4);
        turn_end = pos;
    } else {
        turn_start = 0;
        turn_end = 0;
    }

    /* Write tiles to BKG row 0 */
    VBK_REG = 0;
    for (i = 0; i < 20; i++) {
        set_bkg_tile_xy(i, 0, tiles[i]);
    }

    /* HP color: green > 66%, yellow 33-66%, red < 33% */
    if (player.max_hp > 0) {
        if ((uint16_t)player.hp * 3 > (uint16_t)player.max_hp * 2) {
            hp_pal = PAL_CONSUMABLE;  /* green */
        } else if ((uint16_t)player.hp * 3 > (uint16_t)player.max_hp) {
            hp_pal = PAL_NEUTRAL;     /* yellow */
        } else {
            hp_pal = PAL_HOSTILE;     /* red */
        }
    } else {
        hp_pal = PAL_HOSTILE;
    }

    /* Assign per-section palettes */
    for (i = hp_start; i < hp_end; i++) attrs[i] = hp_pal;
    for (i = ac_start; i < ac_end; i++) attrs[i] = PAL_TERRAIN;    /* light grey/brown */
    for (i = lv_start; i < lv_end; i++) attrs[i] = PAL_UI;          /* white */
    for (i = gold_start; i < gold_end; i++) attrs[i] = PAL_SPECIAL; /* gold/yellow */
    for (i = turn_start; i < turn_end; i++) attrs[i] = PAL_EQUIPMENT; /* blue */

    VBK_REG = 1;
    for (i = 0; i < 20; i++) {
        set_bkg_tile_xy(i, 0, attrs[i]);
    }
    VBK_REG = 0;
}

/* ---- Messages ---- */

void render_message(const char *msg) BANKED {
    uint8_t i;
    uint8_t by;
    uint8_t len;
    char line[21];

    /* Copy message into internal buffer for persistence */
    len = 0;
    while (msg[len] != 0 && len < 40) {
        msg_buf[len] = msg[len];
        len++;
    }
    msg_buf[len] = 0;
    msg_active = 1;

    /* Line 1: first 20 chars */
    for (i = 0; i < 20; i++) {
        line[i] = (i < len) ? msg_buf[i] : ' ';
    }
    line[20] = 0;

    by = (VIEWPORT_H + STATUS_ROWS) & 31;
    VBK_REG = 0;
    for (i = 0; i < 20; i++) {
        set_bkg_tile_xy(i, by, (uint8_t)line[i]);
    }
    VBK_REG = 1;
    for (i = 0; i < 20; i++) {
        set_bkg_tile_xy(i, by, PAL_UI);
    }
    VBK_REG = 0;

    /* Line 2: chars 20..39 */
    for (i = 0; i < 20; i++) {
        line[i] = (i + 20 < len) ? msg_buf[i + 20] : ' ';
    }

    by = (VIEWPORT_H + STATUS_ROWS + 1) & 31;
    VBK_REG = 0;
    for (i = 0; i < 20; i++) {
        set_bkg_tile_xy(i, by, (uint8_t)line[i]);
    }
    VBK_REG = 1;
    for (i = 0; i < 20; i++) {
        set_bkg_tile_xy(i, by, PAL_UI);
    }
    VBK_REG = 0;
}

void render_clear_message(void) BANKED {
    uint8_t i;
    uint8_t by;

    msg_active = 0;
    msg_buf[0] = 0;

    /* Clear the two message rows */
    by = (VIEWPORT_H + STATUS_ROWS) & 31;
    VBK_REG = 0;
    for (i = 0; i < 20; i++) {
        set_bkg_tile_xy(i, by, TILE_BLANK);
        set_bkg_tile_xy(i, (by + 1) & 31, TILE_BLANK);
    }
    VBK_REG = 1;
    for (i = 0; i < 20; i++) {
        set_bkg_tile_xy(i, by, PAL_TERRAIN);
        set_bkg_tile_xy(i, (by + 1) & 31, PAL_TERRAIN);
    }
    VBK_REG = 0;
}

/* ---- Hit flash ---- */

void render_flash_hit(void) BANKED {
    uint16_t flash_pal[4];
    uint8_t f;

    /* Flash palette 0 (terrain) to red/white for 3 frames */
    flash_pal[0] = RGB(31,  4,  4);  /* bright red */
    flash_pal[1] = RGB(31, 12, 12);
    flash_pal[2] = RGB(31, 20, 20);
    flash_pal[3] = RGB(31, 31, 31);  /* white */

    set_bkg_palette(0, 1, flash_pal);
    for (f = 0; f < 3; f++) {
        wait_vbl_done();
    }

    /* Restore original terrain palette */
    set_bkg_palette(0, 1, bg_palettes);
}
