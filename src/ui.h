#ifndef UI_H
#define UI_H
#include "common.h"

/* Message system */
#define MSG_MAX_LEN 20
#define MSG_LOG_SIZE 20

void ui_init(void);
void ui_message(const char *msg);     /* Queue a message */
void ui_message_more(void);           /* Show "--More--" and wait for keypress */
void ui_show_messages(void);          /* Render pending messages */

/* Action menu (A button) */
#define ACTION_NONE      0
#define ACTION_INVENTORY 1
#define ACTION_EAT       2
#define ACTION_QUAFF     3
#define ACTION_READ      4
#define ACTION_ZAP       5
#define ACTION_PICKUP    7
#define ACTION_DROP      8
#define ACTION_WAIT      9
#define ACTION_SAVE_QUIT 10

uint8_t ui_action_menu(void);  /* Blocks until player picks an action */

/* Inventory screen -- returns selected slot or 255 if cancelled */
uint8_t ui_inventory_screen(uint8_t filter_category);  /* 255 = show all */

/* Direction prompt -- returns DIR_* or DIR_NONE if cancelled */
uint8_t ui_direction_prompt(void);

/* Character sheet */
void ui_character_sheet(void);

/* Message history */
void ui_message_history(void);

/* Help screen */
void ui_help_screen(void);

/* Select menu — returns action taken (255 = cancelled) */
#define SEL_MENU_QUIT 4
uint8_t ui_select_menu(void);

/* Yes/No prompt */
uint8_t ui_yes_no(const char *question);  /* returns 1 for yes, 0 for no */
uint8_t ui_pet_choice(void);  /* returns 1=cat, 2=dog */

/* Drawing helpers (used internally and by render) */
void ui_draw_text(uint8_t x, uint8_t y, const char *str, uint8_t pal);
void ui_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pal);

/* Flag checked by render to know when a full redraw is needed */
extern uint8_t ui_needs_redraw;

/* Message staleness: call each turn, clears messages after 3 idle turns */
void ui_message_tick(uint16_t current_turn);

#endif
