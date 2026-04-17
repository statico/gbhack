#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H
#include "common.h"
#include "ui.h"

/* Internal shared state between ui.c and ui_menus.c.
   Not part of the public UI API. */

extern char msg_log[MSG_LOG_SIZE][MSG_MAX_LEN + 1];
extern uint8_t msg_head;
extern uint8_t msg_count;

#endif
