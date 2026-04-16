# --- Paths ---
GBDK_HOME  = $(CURDIR)/../gbdk/
LCC        = $(GBDK_HOME)bin/lcc
PNG2ASSET  = $(GBDK_HOME)bin/png2asset

# --- Project ---
PROJECT_NAME = gbhack
BINS         = build/$(PROJECT_NAME).gbc

# --- Compiler flags ---
LCCFLAGS  = -Wa-l -Wl-m -Wl-j -Wm-yS
LCCFLAGS += -Wm-yC
LCCFLAGS += -Wm-yt0x1B    # MBC5 + RAM + Battery
LCCFLAGS += -Wm-ya4        # 4 SRAM banks (32KB)
LCCFLAGS += -Wm-yo16       # 16 ROM banks (256KB)
LCCFLAGS += -Wm-yn"GBHACK"
LCCFLAGS += -Ilib

# --- Sources ---
SRC_FILES  = $(wildcard src/*.c)
MUS_FILES  = $(wildcard src/music/*.c)
RES_FILES  = $(wildcard res/*.c)
LIB_FILES  = $(wildcard lib/cbtfx.c)

SRC_OBJS  = $(patsubst src/%.c,obj/%.o,$(filter-out src/tile_viewer.c,$(SRC_FILES)))
MUS_OBJS  = $(patsubst src/music/%.c,obj/music_%.o,$(MUS_FILES))
RES_OBJS  = $(patsubst res/%.c,obj/res_%.o,$(RES_FILES))
LIB_OBJS  = $(patsubst lib/%.c,obj/lib_%.o,$(LIB_FILES))

LIB_LINK = $(wildcard lib/*.lib)

OBJS = $(filter-out obj/save_data.o,$(SRC_OBJS)) $(MUS_OBJS) $(RES_OBJS) $(LIB_OBJS)

all: $(BINS)

obj/%.o: src/%.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -c -o $@ $<

# Bank 2: render (alongside tiles)
obj/render.o: src/render.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo2 -c -o $@ $<

# Bank 3: dungeon, fov
obj/dungeon.o: src/dungeon.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo3 -c -o $@ $<

obj/fov.o: src/fov.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo3 -c -o $@ $<

# Bank 4: monsters, pet
obj/monsters.o: src/monsters.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo4 -c -o $@ $<

obj/pet.o: src/pet.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo4 -c -o $@ $<

# Bank 5: inventory, shop
obj/inventory.o: src/inventory.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo5 -c -o $@ $<

obj/shop.o: src/shop.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo5 -c -o $@ $<

# Bank 6: items (functions), save
obj/items.o: src/items.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo6 -c -o $@ $<

obj/save.o: src/save.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo6 -c -o $@ $<

# Bank 7: title screen background image
obj/res_title_bg.o: res/title_bg.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo7 -c -o $@ $<

obj/res_%.o: res/%.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo2 -c -o $@ $<

obj/lib_%.o: lib/%.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -c -o $@ $<

# Bank 8: music (title, dungeon1, dungeon2, death, victory)
obj/music_song_title.o: src/music/song_title.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo8 -c -o $@ $<

obj/music_song_dungeon1.o: src/music/song_dungeon1.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo8 -c -o $@ $<

obj/music_song_dungeon2.o: src/music/song_dungeon2.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo8 -c -o $@ $<

obj/music_song_death.o: src/music/song_death.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo8 -c -o $@ $<

obj/music_song_victory.o: src/music/song_victory.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo8 -c -o $@ $<

# Bank 9: music (boss)
obj/music_song_boss.o: src/music/song_boss.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo9 -c -o $@ $<

obj/save_data.o: src/save_data.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-ba0 -c -o $@ $<

$(BINS): $(OBJS) obj/save_data.o
	@mkdir -p build
	$(LCC) $(LCCFLAGS) -o $@ $^ $(LIB_LINK)

run: $(BINS)
	open -a SameBoy $(BINS)

# --- Tile Viewer ---
VIEWER_BIN = build/tile_viewer.gbc
VIEWER_LCCFLAGS  = -Wa-l -Wl-m -Wl-j -Wm-yS -Wm-yC
VIEWER_LCCFLAGS += -Wm-yt0x1B -Wm-ya1 -Wm-yo4
VIEWER_LCCFLAGS += -Wm-yn"TILEVIEW"

viewer: $(VIEWER_BIN)

obj/tile_viewer.o: src/tile_viewer.c
	@mkdir -p obj
	$(LCC) $(VIEWER_LCCFLAGS) -c -o $@ $<

$(VIEWER_BIN): obj/tile_viewer.o obj/res_tiles.o
	@mkdir -p build
	$(LCC) $(VIEWER_LCCFLAGS) -o $@ $^

clean:
	rm -rf obj/ build/

.PHONY: all run viewer clean
