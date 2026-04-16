#ifndef MONSTERS_H
#define MONSTERS_H
#include "common.h"

extern Monster monsters[MAX_MONSTERS];
extern const MonsterType monster_types[];
extern uint8_t num_monster_types;

void monsters_init(void) BANKED;
void monsters_spawn_for_level(uint8_t level) BANKED;
uint8_t monster_at(uint8_t x, uint8_t y) BANKED;  /* returns index or 255 */
void monsters_update(void) BANKED;  /* all monsters take their turn */
void monster_take_damage(uint8_t idx, uint8_t dmg) BANKED;
void monster_kill(uint8_t idx) BANKED;
uint8_t monster_spawn(uint8_t type_id, uint8_t x, uint8_t y) BANKED;

#endif
