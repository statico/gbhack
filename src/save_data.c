/* This file allocates SRAM variables. Compiled with -Wf-ba0 flag. */
#include <gb/gb.h>
#include <stdint.h>

/* SRAM bank 0 -- save marker and checksum */
__at(0xA000) uint8_t save_marker;
__at(0xA001) uint8_t save_checksum;
