#ifndef SHOP_H
#define SHOP_H
#include "common.h"

#define MAX_SHOP_DEBT_ITEMS 10

extern uint16_t shop_debt;           /* current debt in gold */
extern uint8_t shop_active;          /* 1 if player is in a shop room */
extern uint8_t shop_room_index;      /* which room is the shop */

void shop_init(void) BANKED;
void shop_check_enter(void) BANKED;         /* call when player moves -- detect entering/leaving shop */
void shop_on_pickup(uint8_t item_type_id) BANKED;   /* call when player picks up item in shop */
void shop_on_drop(uint8_t item_type_id) BANKED;     /* call when player drops item in shop */
uint8_t shop_can_leave(void) BANKED;        /* returns 1 if no debt, 0 if debt exists */
void shop_try_leave(void) BANKED;           /* called when player tries to exit shop room */
void shop_pay(void) BANKED;                 /* pay off debt with gold */
void shop_stock_room(uint8_t room_idx) BANKED; /* place items for sale in shop room */

#endif
