#ifndef PLAYER_H
#define PLAYER_H
#include "common.h"

extern Player player;

void player_init(void);
uint8_t player_move(int8_t dx, int8_t dy);  /* move or attack; returns 1 if blocked by wall */
void player_take_damage(uint8_t dmg);
void player_heal(uint8_t amount);
void player_gain_xp(uint16_t xp);
void player_update_hunger(void);
uint8_t player_is_dead(void);
const char *player_get_death_cause(void);
void player_set_death_cause(const char *cause);

#endif
