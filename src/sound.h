#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

/* SFX IDs */
#define SFX_ATTACK    0
#define SFX_PICKUP    1
#define SFX_DOOR      2
#define SFX_STAIRS    3
#define SFX_EAT       4
#define SFX_DEATH     5
#define SFX_LEVELUP   6
#define SFX_SHOP      7
#define SFX_ALTAR      8
#define SFX_PET       9
#define SFX_HIT       10
#define SFX_MISS      11
#define SFX_STEP      12

/* Music track IDs */
#define MUSIC_NONE      0
#define MUSIC_TITLE     1
#define MUSIC_DUNGEON1  2
#define MUSIC_DUNGEON2  3
#define MUSIC_BOSS      4
#define MUSIC_DEATH     5
#define MUSIC_VICTORY   6

/* Toggle flags (1 = enabled, 0 = muted) */
extern uint8_t sound_music_enabled;
extern uint8_t sound_sfx_enabled;

void sound_init(void);
void sound_play_sfx(uint8_t sfx_id);
void sound_play_music(uint8_t track_id);
void sound_stop_music(void);
void sound_update(void);  /* call each frame for hUGEDriver + CBT-FX */
void sound_toggle_music(void);  /* flip music on/off */
void sound_toggle_sfx(void);    /* flip sfx on/off */

#endif
