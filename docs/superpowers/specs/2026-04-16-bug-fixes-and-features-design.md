# gbhack Bug Fixes & Features — Design Spec

8 changes: pet menu redesign, select menu flicker, door generation, corpse drops, item name suffixes, pickaxe mining, lamp removal, help screen text.

## 1. Pet Selection Menu Redesign

**File:** `src/ui.c`, `src/main.c`

Replace `ui_yes_no("Start w/ cat?")` with a new `ui_pet_choice()` function.

- Full-screen box via `ui_draw_box()`
- Centered title "Starting Pet?" (placed at x=3, y=2 or calculated to center on 20-tile width)
- Two vertical options with ">" cursor: "Cat" (cursor=0) and "Dog" (cursor=1)
- Up/down moves cursor, A or Start confirms
- No cancel (B does nothing) — player must choose
- Returns `PET_CAT` (1) or `PET_DOG` (2)
- In `main.c:358-366`, replace the yes/no block with `pet_init(ui_pet_choice())`
- Incremental cursor update (only redraw 2 rows on move, not full screen)

## 2. Select Menu Flicker Fix

**File:** `src/ui.c` — `ui_select_menu()` (lines 894-957) and `select_menu_draw()` (lines 869-892)

Root cause: every up/down press sets `need_draw = 1` which calls `select_menu_draw()`, redrawing the entire screen.

Fix:
- Add `prev_cursor` variable, initialize to `cursor`
- On up/down, do NOT set `need_draw`. Instead, after the input block, check `prev_cursor != cursor`
- If changed: clear ">" at old row (`ty = 2 + prev_cursor * 2`) by drawing " ", draw ">" at new row (`ty = 2 + cursor * 2`). Update `prev_cursor = cursor`
- Keep `need_draw = 1` only for: initial draw, and music/sfx toggle (label text changes)

## 3. Door Generation Fix

**File:** `src/dungeon.c` — `place_doors()` (lines 213-254)

Root cause: any corridor cell adjacent to both a floor cell and another corridor cell becomes a door. When corridors run parallel to room walls, every cell along the edge qualifies.

Fix: replace the `adj_floor && adj_corr` check with a pinch-point test. A valid door position must have:
- Floor (or door) on one axis AND wall on the perpendicular axis:
  - `(N_is_floor || S_is_floor) && (E_is_wall && W_is_wall)`, OR
  - `(E_is_floor || W_is_floor) && (N_is_wall && S_is_wall)`

This ensures doors only appear at narrow entry/exit points into rooms, not along parallel corridor stretches.

## 4. Corpse Drops on Monster Death

**File:** `src/monsters.c` — `monster_kill()` (lines 119-135)

- After clearing the map flag and before deactivating, with 80% probability (`rng_range(0, 99) < 80`), call `item_spawn(30, monsters[idx].x, monsters[idx].y, 1)` to drop a corpse
- The corpse is item type 30 (`ICAT_FOOD`, effect=10 → 100 nutrition)
- Player can pick up and eat corpses via the existing `inventory_pickup()` and `use_food()` paths
- Pet eats corpses automatically via existing `pet_try_eat()` code
- Need to add `#include "items.h"` or declare `item_spawn()` extern in monsters.c if not already available (monsters.c is banked — check that `item_spawn` is callable cross-bank)

### Cross-bank call note

`monster_kill()` is in a BANKED function. `item_spawn()` is also BANKED. GBDK allows banked-to-banked calls via trampoline as long as both are declared with BANKED. Verify the extern declaration is visible.

## 5. Item Name Category Suffixes

**File:** `src/ui.c` — inventory display (lines 508-511), `src/items_data.c`

When an identified potion or scroll is displayed in inventory, the name shows just the effect ("Speed", "Identify") with no category hint.

Fix: in the inventory display code, after copying the item name, append a category suffix for identified potions and scrolls:
- `ICAT_POTION` → append " potion"
- `ICAT_SCROLL` → append " scroll"

Only append when the item is identified (when `IFLAG_IDENTIFIED` is set, since `item_name()` is used). For unidentified items, `item_appearance_name()` already returns "bubbly potion" / "ZELGO MER scroll" style names... actually, checking the code: unidentified scrolls return just the label like "ZELGO MER" without " scroll". So we should append the suffix regardless of identification status when the category is potion or scroll.

Also widen the name truncation limit. Current: `lp < SCREEN_W - 5` (15 chars total for "a) name [E]"). The "[E]" suffix is 4 chars + space = 5. Without [E], we have room for `SCREEN_W - 3 = 17` chars of name. With [E], `SCREEN_W - 8 = 12`. Longest identified name + suffix: "Enchant weapon scroll" = 21 chars — too long. Options:
- Truncate gracefully (cut at screen edge) — acceptable, the important part of the name is at the start
- Change limit to `SCREEN_W - 2` (18 chars) and accept truncation when [E] is present for long names

Go with `SCREEN_W - 2` as the limit. Long names get truncated at the screen edge, which is fine.

## 6. Pickaxe: Equip and Mine Walls

**Files:** `src/inventory.c`, `src/player.c`, `src/items.c`

### Equipping
In `inventory_use()` (line 352 default case): add a special case for pickaxe (type_id == 34). Toggle `IFLAG_EQUIPPED`. Only one pickaxe can be equipped at a time (unequip any other equipped pickaxe first, same pattern as weapons).

### Spawning with durability
In `item_initial_qty()` in `items.c`: add a case for type_id == 34 (pickaxe) returning 300 (uses remaining). Currently tools don't have special qty handling.

### Mining
In `player_move()` in `player.c`: after the `!CELL_IS_PASSABLE(cell)` check that currently returns 1 (blocked), add:
- Check if a pickaxe is equipped (scan inventory for type_id == 34 with IFLAG_EQUIPPED)
- If equipped and terrain is `TERRAIN_WALL`:
  - Set cell to `TERRAIN_CORRIDOR` (carved passage, not room floor)
  - Decrement pickaxe quantity
  - If quantity reaches 0: unequip, remove from inventory, show "Your pickaxe breaks!"
  - Otherwise show "You dig."
  - Play `SFX_DOOR` or similar sound effect
  - Consume a turn (`player.turns++`, `player.nutrition--`)
  - Return 0 (action taken)

### Helper function
Add `inventory_get_equipped_pickaxe()` to inventory.c — returns slot index of equipped pickaxe or 255.

## 7. Remove Lamp from Spawning

**File:** `src/items.c`

Lamp is item 37. Currently tools spawn via `rng_range(33, 39)` (inclusive).

Fix: change to `rng_range(33, 38)`, then if result >= 37 increment by 1. This maps:
- 33→33 (Key), 34→34 (Pickaxe), 35→35 (Amulet), 36→36 (Sack), 37→38 (Stethoscope), 38→39 (Whistle)

Apply the same skip in shop spawning code if it uses the same range.

Leave item_types[37] in the array — removing it would renumber all subsequent items.

## 8. Help Screen: "Press any button"

**File:** `src/ui.c` line 845

Change `"Press any key"` to `"Press any button"`.
