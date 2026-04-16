#ifndef SAVE_H
#define SAVE_H
#include "common.h"

void save_game(void) BANKED;
uint8_t save_load(void) BANKED;      /* returns 1 if valid save exists, 0 if not */
void save_delete(void) BANKED;       /* wipe save (permadeath) */
void save_write_bones(void) BANKED;  /* save bones data on death */
void save_write_hiscore(uint16_t score) BANKED;
uint16_t save_get_hiscore(void) BANKED;

#endif
