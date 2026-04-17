#ifndef PET_H
#define PET_H
#include "common.h"

extern uint8_t pet_index;          /* index into monsters[] array, 255 if no pet */
extern uint8_t pet_away_turns;     /* turns pet has been far from player */

void pet_init(uint8_t type) BANKED;       /* 1=cat, 2=dog. Spawns pet near player. */
void pet_update(void) BANKED;             /* call each turn after monsters_update */
uint8_t pet_is_alive(void) BANKED;        /* returns 1 if pet exists and is active */
void pet_check_feral(void) BANKED;        /* check if pet should go feral */

/* Re-spawn a pet near the player after stair traversal, preserving hp/status.
   Does NOT print a welcome message. */
void pet_respawn_after_stairs(uint8_t type, uint8_t hp, uint8_t status) BANKED;

#endif
