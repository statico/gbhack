#ifndef INVENTORY_H
#define INVENTORY_H
#include "common.h"

extern Item inventory[MAX_INVENTORY];
extern uint8_t inventory_count;

void inventory_init(void) BANKED;
uint8_t inventory_add(uint8_t type_id, uint8_t qty, uint8_t flags) BANKED;  /* returns slot or 255 if full */
void inventory_remove(uint8_t slot) BANKED;
uint8_t inventory_find(uint8_t type_id) BANKED;  /* returns slot or 255 */
void inventory_pickup(void) BANKED;  /* pick up item at player position */
void inventory_drop(uint8_t slot) BANKED;  /* drop item at player position */
void inventory_use(uint8_t slot) BANKED;  /* use/eat/quaff/read/equip */
void inventory_equip(uint8_t slot) BANKED;
void inventory_unequip(uint8_t slot) BANKED;
uint8_t inventory_get_weapon_damage(void) BANKED;  /* dice sides of equipped weapon, 0 if none */
int8_t inventory_get_armor_ac(void) BANKED;  /* total AC bonus from equipped armor */

#endif
