#include <gb/gb.h>
#include <stdint.h>
#include "sound.h"
#include "cbtfx.h"
#include "hUGEDriver.h"

/*
 * CBT-FX sound effect data format:
 *
 * Header:
 *   byte 0: ch_used flags + priority
 *           bit 7 = use CH2 (pulse), bit 5 = use CH4 (noise)
 *           bits 0-3 = priority (higher overrides lower)
 *   byte 1: number of frames
 *
 * Per frame (when CH2 only, 0x80):
 *   [repeater, NR21_duty, volume_byte, NR23_freq_lo, NR24_freq_hi]
 *   repeater bits 0-6 = frame duration, bit 7 = panning byte follows
 *   volume_byte upper nibble = CH2 envelope (NR22 & 0xF0)
 *
 * Per frame (when CH4 only, 0x20):
 *   [repeater, volume_byte, NR43_noise_freq]
 *   volume_byte lower nibble << 4 = CH4 volume (NR42)
 *
 * Per frame (when CH2+CH4, 0xA0):
 *   [repeater, NR21_duty, volume_byte, NR23_freq_lo, NR24_freq_hi, NR43_noise_freq]
 */

/* --- SFX data arrays ---
 *
 * Each array is: [ch_used|priority, num_frames, frame_data...]
 */

/* ATTACK: sharp noise burst (CH4 only, priority 3) */
static const unsigned char sfx_attack[] = {
    0x23, 3,
    /* frame 0: loud noise hit */
    0x02, 0x0F, 0x30,
    /* frame 1: mid noise */
    0x02, 0x0C, 0x41,
    /* frame 2: fade out */
    0x01, 0x06, 0x50,
};

/* PICKUP: rising tone (CH2 only, priority 3) */
static const unsigned char sfx_pickup[] = {
    0x83, 4,
    /* frame 0: low tone */
    0x00, 0x80, 0xF0, 0x00, 0x85,
    /* frame 1: mid tone */
    0x00, 0x80, 0xE0, 0x40, 0x86,
    /* frame 2: higher tone */
    0x00, 0x80, 0xC0, 0x80, 0x86,
    /* frame 3: highest, fade */
    0x00, 0x80, 0x80, 0xC0, 0x87,
};

/* DOOR: creaking sweep (CH2 only, priority 3) */
static const unsigned char sfx_door[] = {
    0x83, 4,
    /* frame 0: low creak start */
    0x02, 0xC0, 0xF0, 0x20, 0x84,
    /* frame 1: sweep up */
    0x02, 0xC0, 0xD0, 0x60, 0x85,
    /* frame 2: sweep down */
    0x02, 0xC0, 0xA0, 0x30, 0x84,
    /* frame 3: fade */
    0x01, 0xC0, 0x60, 0x50, 0x85,
};

/* STAIRS: descending tone (CH2 only, priority 2) */
static const unsigned char sfx_stairs[] = {
    0x82, 5,
    /* frame 0: high start */
    0x00, 0x80, 0xF0, 0xC0, 0x87,
    /* frame 1 */
    0x00, 0x80, 0xD0, 0x80, 0x86,
    /* frame 2 */
    0x00, 0x80, 0xB0, 0x40, 0x86,
    /* frame 3 */
    0x00, 0x80, 0x90, 0x00, 0x85,
    /* frame 4: low end */
    0x00, 0x80, 0x60, 0x80, 0x84,
};

/* EAT: chomping sound (CH4 only, priority 1) */
static const unsigned char sfx_eat[] = {
    0x21, 4,
    /* frame 0: chomp */
    0x00, 0x0D, 0x24,
    /* frame 1: silence */
    0x01, 0x02, 0x30,
    /* frame 2: chomp */
    0x00, 0x0C, 0x24,
    /* frame 3: fade */
    0x00, 0x03, 0x40,
};

/* DEATH: descending "BOoooop" tone (CH2 only, priority 5)
 * Starts high and smoothly descends with a long fade out */
static const unsigned char sfx_death[] = {
    0x85, 8,
    /* frame 0: high start "B" */
    0x02, 0x80, 0xF0, 0xC0, 0x87,
    /* frame 1: start descending "Oo" */
    0x02, 0x80, 0xF0, 0x80, 0x87,
    /* frame 2: "oo" */
    0x03, 0x80, 0xE0, 0x40, 0x87,
    /* frame 3: "oo" lower */
    0x03, 0x80, 0xD0, 0x00, 0x86,
    /* frame 4: "oo" even lower */
    0x04, 0xC0, 0xB0, 0x80, 0x85,
    /* frame 5: "p" fading */
    0x04, 0xC0, 0x80, 0x00, 0x85,
    /* frame 6: tail fade */
    0x05, 0xC0, 0x50, 0x80, 0x84,
    /* frame 7: silence fade */
    0x06, 0xC0, 0x20, 0x00, 0x84,
};

/* LEVELUP: triumphant ascending tones (CH2 only, priority 4) */
static const unsigned char sfx_levelup[] = {
    0x84, 6,
    /* frame 0: note 1 low */
    0x02, 0x40, 0xF0, 0x00, 0x85,
    /* frame 1: note 2 */
    0x02, 0x40, 0xF0, 0x40, 0x86,
    /* frame 2: note 3 */
    0x02, 0x40, 0xF0, 0x80, 0x86,
    /* frame 3: note 4 higher */
    0x02, 0x40, 0xF0, 0xC0, 0x87,
    /* frame 4: note 5 highest */
    0x03, 0x40, 0xF0, 0xE0, 0x87,
    /* frame 5: ring out */
    0x04, 0x40, 0x80, 0xE0, 0x87,
};

/* SHOP: cash register ding (CH2 only, priority 2) */
static const unsigned char sfx_shop[] = {
    0x82, 3,
    /* frame 0: bright ding */
    0x01, 0x40, 0xF0, 0xD0, 0x87,
    /* frame 1: ring */
    0x02, 0x40, 0xC0, 0xD0, 0x87,
    /* frame 2: fade */
    0x03, 0x40, 0x60, 0xD0, 0x87,
};

/* ALTAR: ethereal chime (CH2 only, priority 3) */
static const unsigned char sfx_altar[] = {
    0x83, 6,
    /* frame 0: soft high tone */
    0x02, 0x00, 0xA0, 0xE0, 0x87,
    /* frame 1: warble up */
    0x02, 0x00, 0xB0, 0xF0, 0x87,
    /* frame 2: warble down */
    0x02, 0x00, 0xA0, 0xD0, 0x87,
    /* frame 3: warble up */
    0x02, 0x00, 0x90, 0xF0, 0x87,
    /* frame 4: warble down */
    0x03, 0x00, 0x70, 0xD0, 0x87,
    /* frame 5: fade */
    0x04, 0x00, 0x30, 0xE0, 0x87,
};

/* PET: short bark (CH4 only, priority 1) */
static const unsigned char sfx_pet[] = {
    0x21, 3,
    /* frame 0: bark attack */
    0x00, 0x0F, 0x33,
    /* frame 1: bark body */
    0x01, 0x0B, 0x36,
    /* frame 2: bark tail */
    0x00, 0x04, 0x44,
};

/* HIT: impact thud (CH4 only, priority 3) */
static const unsigned char sfx_hit[] = {
    0x23, 3,
    /* frame 0: heavy impact */
    0x02, 0x0F, 0x20,
    /* frame 1: rumble */
    0x02, 0x0C, 0x30,
    /* frame 2: fade */
    0x02, 0x06, 0x42,
};

/* MISS: whoosh (CH4 only, priority 2) */
static const unsigned char sfx_miss[] = {
    0x22, 3,
    /* frame 0: airy start */
    0x01, 0x0A, 0x55,
    /* frame 1: sweep through */
    0x02, 0x07, 0x50,
    /* frame 2: tail off */
    0x01, 0x03, 0x44,
};

/* STEP: footstep tap (CH4 only, priority 1) */
static const unsigned char sfx_step[] = {
    0x21, 2,
    /* frame 0: audible tap */
    0x01, 0x08, 0x61,
    /* frame 1: quick fade */
    0x00, 0x03, 0x70,
};

/* SEARCH: swoosh (CH4 only, priority 2) */
static const unsigned char sfx_search[] = {
    0x22, 3,
    /* frame 0: start */
    0x01, 0x09, 0x52,
    /* frame 1: whoosh */
    0x02, 0x07, 0x42,
    /* frame 2: fade */
    0x01, 0x03, 0x50,
};

/* SFX lookup table */
static const unsigned char * const sfx_table[] = {
    sfx_attack,   /* SFX_ATTACK   0 */
    sfx_pickup,   /* SFX_PICKUP   1 */
    sfx_door,     /* SFX_DOOR     2 */
    sfx_stairs,   /* SFX_STAIRS   3 */
    sfx_eat,      /* SFX_EAT      4 */
    sfx_death,    /* SFX_DEATH    5 */
    sfx_levelup,  /* SFX_LEVELUP  6 */
    sfx_shop,     /* SFX_SHOP     7 */
    sfx_altar,    /* SFX_ALTAR    8 */
    sfx_pet,      /* SFX_PET      9 */
    sfx_hit,      /* SFX_HIT      10 */
    sfx_miss,     /* SFX_MISS     11 */
    sfx_step,     /* SFX_STEP     12 */
    sfx_search,   /* SFX_SEARCH   13 */
};

#define SFX_COUNT 14

/* Song data (in banked ROM) */
extern const hUGESong_t song_title;
extern const hUGESong_t song_dungeon1;
extern const hUGESong_t song_dungeon2;
extern const hUGESong_t song_boss;
extern const hUGESong_t song_death;
extern const hUGESong_t song_victory;

/* Music state */
static uint8_t music_playing = 0;
static uint8_t music_bank = 8;

/* Song pointers indexed by MUSIC_* track IDs */
static const hUGESong_t * const song_ptrs[] = {
    0,              /* MUSIC_NONE    0 */
    &song_title,    /* MUSIC_TITLE   1 */
    &song_dungeon1, /* MUSIC_DUNGEON1 2 */
    &song_dungeon2, /* MUSIC_DUNGEON2 3 */
    &song_boss,     /* MUSIC_BOSS    4 */
    &song_death,    /* MUSIC_DEATH   5 */
    &song_victory,  /* MUSIC_VICTORY 6 */
};

/* Bank numbers for each track */
static const uint8_t song_banks[] = {
    0,  /* MUSIC_NONE */
    8,  /* MUSIC_TITLE */
    8,  /* MUSIC_DUNGEON1 */
    8,  /* MUSIC_DUNGEON2 */
    9,  /* MUSIC_BOSS */
    8,  /* MUSIC_DEATH */
    8,  /* MUSIC_VICTORY */
};

static uint8_t vbl_registered = 0;

void sound_init(void) {
    /* Enable audio hardware */
    NR52_REG = 0x80; /* master sound on */
    NR51_REG = 0xFF; /* all channels to both speakers */
    NR50_REG = 0x77; /* max volume both sides */
    music_playing = 0;

    /* Register VBlank handler so music ticks every frame automatically,
       even during heavy rendering/AI/FOV computation. */
    if (!vbl_registered) {
        add_VBL(sound_update);
        vbl_registered = 1;
    }
}

void sound_play_sfx(uint8_t sfx_id) {
    if (sfx_id < SFX_COUNT) {
        CBTFX_init(sfx_table[sfx_id]);
    }
}

void sound_play_music(uint8_t track_id) {
    uint8_t saved_bank;
    if (track_id == MUSIC_NONE || track_id > MUSIC_VICTORY) {
        sound_stop_music();
        return;
    }
    saved_bank = CURRENT_BANK;
    music_bank = song_banks[track_id];
    SWITCH_ROM(music_bank);
    hUGE_init(song_ptrs[track_id]);
    SWITCH_ROM(saved_bank);
    music_playing = 1;
}

void sound_stop_music(void) {
    music_playing = 0;
    /* Silence all channels to cut any lingering sound */
    NR12_REG = 0x00; NR14_REG = 0x80;
    NR22_REG = 0x00; NR24_REG = 0x80;
    NR32_REG = 0x00;
    NR42_REG = 0x00; NR44_REG = 0x80;
}

void sound_update(void) {
    uint8_t saved_bank;

    /* Process CBT-FX sound effects (always, even without music) */
    CBTFX_update();

    /* Process hUGEDriver music tick */
    if (music_playing) {
        saved_bank = CURRENT_BANK;
        SWITCH_ROM(music_bank);
        hUGE_dosound();
        SWITCH_ROM(saved_bank);
    }
}
