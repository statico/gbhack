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
LCCFLAGS += -Wm-yo8        # 8 ROM banks (128KB)
LCCFLAGS += -Wm-yn"GBHACK"
LCCFLAGS += -Ilib

# --- Sources ---
SRC_FILES = $(wildcard src/*.c)
RES_FILES = $(wildcard res/*.c)
LIB_FILES = $(wildcard lib/cbtfx.c)

SRC_OBJS  = $(patsubst src/%.c,obj/%.o,$(SRC_FILES))
RES_OBJS  = $(patsubst res/%.c,obj/res_%.o,$(RES_FILES))
LIB_OBJS  = $(patsubst lib/%.c,obj/lib_%.o,$(LIB_FILES))

LIB_LINK = $(wildcard lib/*.lib)

OBJS = $(filter-out obj/save_data.o,$(SRC_OBJS)) $(RES_OBJS) $(LIB_OBJS)

all: $(BINS)

obj/%.o: src/%.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -c -o $@ $<

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

obj/res_%.o: res/%.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-bo2 -c -o $@ $<

obj/lib_%.o: lib/%.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -c -o $@ $<

obj/save_data.o: src/save_data.c
	@mkdir -p obj
	$(LCC) $(LCCFLAGS) -Wf-ba0 -c -o $@ $<

$(BINS): $(OBJS) obj/save_data.o
	@mkdir -p build
	$(LCC) $(LCCFLAGS) -o $@ $^ $(LIB_LINK)

run: $(BINS)
	open -a SameBoy $(BINS)

clean:
	rm -rf obj/ build/

.PHONY: all run clean
