# gbhack Bug Fixes & Features — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix 8 bugs/missing features: pet menu, select menu flicker, door generation, corpse drops, item name suffixes, pickaxe mining, lamp removal, help screen text.

**Architecture:** Each task is a self-contained change to 1-3 C source files. No new files needed. All changes are in existing GBDK/Game Boy C source. No test suite exists — verification is `make` compiles cleanly.

**Tech Stack:** GBDK-2020, C (Game Boy), LCC compiler

**Build command:** `make` from project root. Output: `build/gbhack.gbc`

**Bank layout reference (cross-bank calls must use BANKED):**
- Bank 0: ui.c, items_data.c, main.c, player.c (unbanked, always accessible)
- Bank 3: dungeon.c, fov.c
- Bank 4: monsters.c, pet.c
- Bank 5: inventory.c, shop.c
- Bank 6: items.c, save.c

---

### Task 1: Help screen — "Press any button"

**Files:**
- Modify: `src/ui.c:845`

- [ ] **Step 1: Change the text**

In `src/ui.c`, line 845, change:
```c
    ui_draw_text(3, 16, "Press any key", PAL_UI);
```
to:
```c
    ui_draw_text(2, 16, "Press any button", PAL_UI);
```
(x moved from 3 to 2 to keep it roughly centered — "Press any button" is 16 chars on a 20-tile-wide screen, so x=2 centers it)

- [ ] **Step 2: Build**

Run: `make`
Expected: clean compile, `build/gbhack.gbc` produced

- [ ] **Step 3: Commit**

```bash
git add src/ui.c
git commit -m "Fix help screen: 'Press any key' -> 'Press any button'"
```

---

### Task 2: Select menu flicker fix

**Files:**
- Modify: `src/ui.c:894-957` — `ui_select_menu()`

The root cause: every up/down press sets `need_draw = 1`, which calls `select_menu_draw()` to redraw the entire screen. The inventory screen doesn't flicker because it only redraws the two affected cursor rows.

- [ ] **Step 1: Add prev_cursor tracking and incremental cursor update**

In `src/ui.c`, replace the `ui_select_menu()` function (lines 894-957) with:

```c
uint8_t ui_select_menu(void) {
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
            /* Incremental update: only redraw the two affected rows */
            ty = 2 + prev_cursor * 2;
            ui_draw_text(2, ty, " ", PAL_UI);
            ty = 2 + cursor * 2;
            ui_draw_text(2, ty, ">", PAL_UI);
        }
        prev_cursor = cursor;

        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP) {
            if (cursor > 0) {
                cursor--;
            } else {
                cursor = SEL_COUNT - 1;
            }
        }

        if (joy_pressed & J_DOWN) {
            if (cursor < SEL_COUNT - 1) {
                cursor++;
            } else {
                cursor = 0;
            }
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
                need_draw = 1;  /* refresh to show new label */
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
            return 255;  /* cancelled */
        }
    }
}
```

Key changes vs. original:
- Added `prev_cursor` variable
- Up/down no longer sets `need_draw = 1`
- New `else if (prev_cursor != cursor)` block clears old cursor, draws new cursor
- `prev_cursor = cursor` set after drawing, before input
- Music/SFX toggle still uses `need_draw = 1` because the label text changes

- [ ] **Step 2: Build**

Run: `make`
Expected: clean compile

- [ ] **Step 3: Commit**

```bash
git add src/ui.c
git commit -m "Fix select menu flicker: incremental cursor update"
```

---

### Task 3: Pet selection menu redesign

**Files:**
- Modify: `src/ui.c` — add `ui_pet_choice()` function, near `ui_yes_no()`
- Modify: `src/ui.h` — declare `ui_pet_choice()`
- Modify: `src/main.c:357-366` — replace yes/no call

- [ ] **Step 1: Add ui_pet_choice() to ui.c**

In `src/ui.c`, add this function after `ui_yes_no()` (after line 1016):

```c
uint8_t ui_pet_choice(void) {
    uint8_t cursor;  /* 0 = Cat, 1 = Dog */
    uint8_t prev_cursor;

    cursor = 0;
    prev_cursor = 0;

    /* Draw the full screen once */
    ui_draw_box(0, 0, SCREEN_W, SCREEN_H, PAL_UI);
    ui_draw_text(3, 2, "Starting Pet?", PAL_UI);

    ui_draw_text(7, 6, "Cat", PAL_UI);
    ui_draw_text(7, 8, "Dog", PAL_UI);

    /* Draw initial cursor */
    ui_draw_text(5, 6, ">", PAL_UI);
    ui_draw_text(5, 8, " ", PAL_UI);

    for (;;) {
        if (prev_cursor != cursor) {
            /* Clear old cursor, draw new */
            ui_draw_text(5, 6 + prev_cursor * 2, " ", PAL_UI);
            ui_draw_text(5, 6 + cursor * 2, ">", PAL_UI);
        }
        prev_cursor = cursor;

        wait_vbl_done();
        input_update();

        if (joy_pressed & J_UP) {
            cursor = 0;
        }
        if (joy_pressed & J_DOWN) {
            cursor = 1;
        }

        if ((joy_pressed & J_A) || (joy_pressed & J_START)) {
            ui_needs_redraw = 1;
            /* 1 = PET_CAT, 2 = PET_DOG */
            return cursor + 1;
        }
        /* No B cancel — player must choose */
    }
}
```

Layout on 20-tile-wide screen:
- "Starting Pet?" is 13 chars → x=3 centers it (3 + 13 + 4 = 20)
- "Cat" and "Dog" at x=7 with cursor ">" at x=5

- [ ] **Step 2: Declare in ui.h**

In `src/ui.h`, after the `ui_yes_no` declaration (line 48), add:

```c
uint8_t ui_pet_choice(void);  /* returns 1=cat, 2=dog */
```

- [ ] **Step 3: Update main.c to use new menu**

In `src/main.c`, replace lines 357-366:

```c
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
```

with:

```c
    /* Pet choice */
    pet_init(ui_pet_choice());
```

- [ ] **Step 4: Build**

Run: `make`
Expected: clean compile

- [ ] **Step 5: Commit**

```bash
git add src/ui.c src/ui.h src/main.c
git commit -m "Redesign pet selection: centered menu with Cat/Dog options"
```

---

### Task 4: Door generation fix

**Files:**
- Modify: `src/dungeon.c:213-254` — `place_doors()`

Root cause: any corridor cell adjacent to both floor and another corridor becomes a door. When a corridor runs parallel to a room wall, every cell along the edge qualifies, creating 5-8 consecutive doors.

Fix: a valid door must be a pinch point — floor on one axis, wall on the perpendicular axis.

- [ ] **Step 1: Replace the door placement logic**

In `src/dungeon.c`, replace the `place_doors()` function (lines 213-254) with:

```c
static void place_doors(void) {
    uint8_t x, y;
    uint8_t terrain;
    uint8_t n_terrain, s_terrain, e_terrain, w_terrain;
    uint8_t ns_has_floor, ew_has_floor;
    uint8_t ns_both_wall, ew_both_wall;

    /*
     * Place doors at pinch points: corridor cells where one axis
     * connects to room floor and the perpendicular axis is walled.
     * This prevents long runs of doors along parallel corridors.
     */
    for (y = 1; y < MAP_HEIGHT - 1; y++) {
        for (x = 1; x < MAP_WIDTH - 1; x++) {
            terrain = CELL_TERRAIN(dungeon_get_cell(x, y));
            if (terrain != TERRAIN_CORRIDOR) continue;

            n_terrain = CELL_TERRAIN(dungeon_get_cell(x, y - 1));
            s_terrain = CELL_TERRAIN(dungeon_get_cell(x, y + 1));
            e_terrain = CELL_TERRAIN(dungeon_get_cell(x + 1, y));
            w_terrain = CELL_TERRAIN(dungeon_get_cell(x - 1, y));

            ns_has_floor = (n_terrain == TERRAIN_FLOOR ||
                            s_terrain == TERRAIN_FLOOR);
            ew_has_floor = (e_terrain == TERRAIN_FLOOR ||
                            w_terrain == TERRAIN_FLOOR);
            ns_both_wall = (n_terrain == TERRAIN_WALL &&
                            s_terrain == TERRAIN_WALL);
            ew_both_wall = (e_terrain == TERRAIN_WALL &&
                            w_terrain == TERRAIN_WALL);

            /*
             * Valid door: floor on one axis, wall on the other.
             * N/S has floor + E/W both wall = horizontal room edge
             * E/W has floor + N/S both wall = vertical room edge
             */
            if ((ns_has_floor && ew_both_wall) ||
                (ew_has_floor && ns_both_wall)) {
                dungeon_set_cell(x, y, TERRAIN_DOOR_CLOSED);
            }
        }
    }
}
```

- [ ] **Step 2: Build**

Run: `make`
Expected: clean compile

- [ ] **Step 3: Commit**

```bash
git add src/dungeon.c
git commit -m "Fix door generation: only place doors at pinch points"
```

---

### Task 5: Corpse drops on monster death

**Files:**
- Modify: `src/monsters.c:1-9` — add include
- Modify: `src/monsters.c:119-135` — `monster_kill()`
- Modify: `src/items.c:162-164` — exclude corpse from random food spawns

- [ ] **Step 1: Add items.h include to monsters.c**

In `src/monsters.c`, after line 7 (`#include "rng.h"`), add:

```c
#include "items.h"
```

(Both `monster_kill()` and `item_spawn()` are BANKED functions. GBDK handles cross-bank calls via trampoline when the callee is declared with BANKED in its header, which `items.h:19` does.)

- [ ] **Step 2: Add corpse drop to monster_kill()**

In `src/monsters.c`, replace the `monster_kill()` function (lines 119-135) with:

```c
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

    /* 80% chance to drop a corpse */
    if (rng_range(0, 99) < 80) {
        item_spawn(30, monsters[idx].x, monsters[idx].y, 1);
    }

    monsters[idx].active = 0;
}
```

- [ ] **Step 3: Exclude corpse from random floor item spawns**

In `src/items.c`, replace lines 162-164:

```c
    } else if (roll < 95) {
        /* Food: 10% */
        type_id = rng_range(28, 31);
```

with:

```c
    } else if (roll < 95) {
        /* Food: 10% — skip corpse (30), only ration/apple/tin */
        type_id = rng_range(28, 30);
        if (type_id == 30) type_id = 31;  /* remap corpse -> tin */
```

Also apply the same fix in `src/shop.c`, replace lines 68-70:

```c
    } else if (roll < 95) {
        /* Food: 10% */
        type_id = rng_range(28, 31);
```

with:

```c
    } else if (roll < 95) {
        /* Food: 10% — skip corpse (30) */
        type_id = rng_range(28, 30);
        if (type_id == 30) type_id = 31;
```

- [ ] **Step 4: Build**

Run: `make`
Expected: clean compile

- [ ] **Step 5: Commit**

```bash
git add src/monsters.c src/items.c src/shop.c
git commit -m "Add corpse drops on monster death (80% chance)"
```

---

### Task 6: Item name category suffixes

**Files:**
- Modify: `src/ui.c:494-521` — inventory display line building

When identified, potions show "Speed" instead of "Speed pot." and scrolls show "Identify" instead of "Identify scrl". Unidentified potions already show "bubbly potion" (has the word), but unidentified scrolls show just "ZELGO MER" (no category hint).

Fix: append " pot." for identified potions, " scrl" for all scrolls. Skip if line is too long.

- [ ] **Step 1: Replace inventory display line building**

In `src/ui.c`, replace lines 494-521 (inside `ui_inventory_screen()`) with:

```c
            /* Build the display line: "a) Item name [E]" */
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

            /* Copy name */
            for (i = 0; name[i] != '\0' && lp < SCREEN_W - 2; i++) {
                line[lp++] = name[i];
            }

            /* Category suffix: identified potions + all scrolls */
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

            /* Equipped marker */
            if (inventory[slot].flags & IFLAG_EQUIPPED) {
                if (lp + 4 <= SCREEN_W) {
                    line[lp++] = ' ';
                    line[lp++] = '[';
                    line[lp++] = 'E';
                    line[lp++] = ']';
                }
            }

            line[lp] = '\0';
```

Display examples:
- Identified: "a) Speed pot." (14), "a) Healing pot." (16), "a) Identify scrl" (18)
- Unidentified: "a) bubbly potion" (17, no suffix — already has "potion"), "a) ZELGO MER scrl" (19)
- Long names truncate gracefully — the `lp + 5 <= SCREEN_W - 2` guard skips the suffix if it won't fit

- [ ] **Step 2: Build**

Run: `make`
Expected: clean compile

- [ ] **Step 3: Commit**

```bash
git add src/ui.c
git commit -m "Add category suffix to potion/scroll names in inventory"
```

---

### Task 7: Pickaxe — equip and mine walls

**Files:**
- Modify: `src/inventory.c:322-356` — `inventory_use()` to handle pickaxe equip
- Modify: `src/inventory.c` — add `inventory_get_equipped_pickaxe()`
- Modify: `src/inventory.h` — declare new function
- Modify: `src/player.c:85-88` — add mining on wall bump
- Modify: `src/items.c:173-190` — `item_initial_qty()` for pickaxe durability

- [ ] **Step 1: Set pickaxe initial durability to 300**

In `src/items.c`, in the `item_initial_qty()` function (lines 173-190), add a case for pickaxe before the `return 1` at line 189. Replace:

```c
    if (cat == ICAT_WAND) {
        if (type_id == 27) return rng_range(1, 3);
        if (type_id == 26) return rng_range(4, 8);
        return rng_range(3, 6);
    }
    return 1;
```

with:

```c
    if (cat == ICAT_WAND) {
        if (type_id == 27) return rng_range(1, 3);
        if (type_id == 26) return rng_range(4, 8);
        return rng_range(3, 6);
    }
    if (type_id == 34) {  /* Pickaxe: durability */
        return 255;  /* max uint8_t — closest to 300 in a single byte */
    }
    return 1;
```

Note: `quantity` is `uint8_t` so max is 255. Use 255 as durability instead of 300 — still very generous. Alternatively, if the quantity field could be widened: but that would change the Item struct and affect save format. 255 swings is plenty.

- [ ] **Step 2: Add pickaxe equip toggle in inventory_use()**

In `src/inventory.c`, in the `inventory_use()` function, replace lines 343-355:

```c
        case ICAT_WEAPON:
        case ICAT_ARMOR:
            /* Toggle equip */
            if (inventory[slot].flags & IFLAG_EQUIPPED) {
                inventory_unequip(slot);
            } else {
                inventory_equip(slot);
            }
            break;
        default:
            ui_message("Can't use that.");
            break;
```

with:

```c
        case ICAT_WEAPON:
        case ICAT_ARMOR:
            /* Toggle equip */
            if (inventory[slot].flags & IFLAG_EQUIPPED) {
                inventory_unequip(slot);
            } else {
                inventory_equip(slot);
            }
            break;
        case ICAT_TOOL:
            if (inventory[slot].type_id == 34) {
                /* Pickaxe: toggle equip like a weapon */
                if (inventory[slot].flags & IFLAG_EQUIPPED) {
                    inventory_unequip(slot);
                } else {
                    /* Unequip any other pickaxe first */
                    {
                        uint8_t pi;
                        for (pi = 0; pi < MAX_INVENTORY; pi++) {
                            if (inventory[pi].type_id == 34 &&
                                (inventory[pi].flags & IFLAG_EQUIPPED)) {
                                inventory[pi].flags &= ~IFLAG_EQUIPPED;
                            }
                        }
                    }
                    inventory[slot].flags |= IFLAG_EQUIPPED;
                    ui_message("You wield the pick.");
                }
            } else {
                ui_message("Can't use that.");
            }
            break;
        default:
            ui_message("Can't use that.");
            break;
```

- [ ] **Step 3: Add inventory_get_equipped_pickaxe() helper**

In `src/inventory.c`, after `inventory_get_armor_ac()` (near the end of the file), add:

```c
uint8_t inventory_get_equipped_pickaxe(void) BANKED {
    uint8_t i;
    for (i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].type_id == 34 &&
            (inventory[i].flags & IFLAG_EQUIPPED)) {
            return i;
        }
    }
    return 255;
}
```

- [ ] **Step 4: Declare in inventory.h**

In `src/inventory.h`, after line 18 (`int8_t inventory_get_armor_ac(void) BANKED;`), add:

```c
uint8_t inventory_get_equipped_pickaxe(void) BANKED;  /* returns slot or 255 */
```

- [ ] **Step 5: Add mining logic to player_move()**

In `src/player.c`, add the include at the top (after existing includes):

```c
#include "inventory.h"
```

Then replace lines 85-88:

```c
    /* Check passability - return 1 to signal wall bump */
    if (!CELL_IS_PASSABLE(cell)) {
        return 1;
    }
```

with:

```c
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
```

This requires `inventory.h` for `inventory_get_equipped_pickaxe()`, `inventory[]`, and `inventory_remove()`. Both `player.c` (bank 0) and `inventory.c` (bank 5) — cross-bank calls via BANKED are fine.

- [ ] **Step 6: Build**

Run: `make`
Expected: clean compile

- [ ] **Step 7: Commit**

```bash
git add src/inventory.c src/inventory.h src/player.c src/items.c
git commit -m "Add pickaxe: equip to mine walls, breaks after 255 uses"
```

---

### Task 8: Remove lamp from spawning

**Files:**
- Modify: `src/items.c:165-168` — tool spawn range
- Modify: `src/shop.c:72-73` — shop tool spawn range

- [ ] **Step 1: Skip lamp in floor item spawning**

In `src/items.c`, replace lines 165-168:

```c
    } else {
        /* Tool: 5% */
        type_id = rng_range(33, 39);
    }
```

with:

```c
    } else {
        /* Tool: 5% — skip lamp (37) */
        type_id = rng_range(33, 38);
        if (type_id >= 37) type_id++;
    }
```

This maps: 33→33, 34→34, 35→35, 36→36, 37→38, 38→39. Lamp (37) is never selected.

- [ ] **Step 2: Skip lamp in shop spawning**

In `src/shop.c`, replace lines 72-73:

```c
        /* Tool: 5% */
        type_id = rng_range(33, 39);
```

with:

```c
        /* Tool: 5% — skip lamp (37) */
        type_id = rng_range(33, 38);
        if (type_id >= 37) type_id++;
```

- [ ] **Step 3: Build**

Run: `make`
Expected: clean compile

- [ ] **Step 4: Commit**

```bash
git add src/items.c src/shop.c
git commit -m "Remove lamp from item spawn pools"
```
