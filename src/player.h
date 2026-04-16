#ifndef PLAYER_H
#define PLAYER_H
#include "common.h"

extern Player player;

void player_init(void);
void player_move(int8_t dx, int8_t dy);  /* move or attack */
void player_take_damage(uint8_t dmg);
void player_heal(uint8_t amount);
void player_gain_xp(uint16_t xp);
void player_update_hunger(void);
uint8_t player_is_dead(void);

#endif
