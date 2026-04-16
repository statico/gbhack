#ifndef RNG_H
#define RNG_H

#include <stdint.h>

void rng_seed(uint16_t seed);
uint16_t rng_next(void);
uint8_t rng_range(uint8_t min, uint8_t max);  // inclusive
uint8_t rng_roll(uint8_t dice, uint8_t sides); // roll NdS

#endif
