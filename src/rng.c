#include "rng.h"

static uint16_t rng_state = 1;

void rng_seed(uint16_t seed) {
    rng_state = seed ? seed : 1;
}

uint16_t rng_next(void) {
    rng_state ^= rng_state << 7;
    rng_state ^= rng_state >> 9;
    rng_state ^= rng_state << 8;
    return rng_state;
}

uint8_t rng_range(uint8_t min, uint8_t max) {
    uint8_t range;
    if (min >= max) return min;
    range = max - min + 1;
    return min + (rng_next() % range);
}

uint8_t rng_roll(uint8_t dice, uint8_t sides) {
    uint8_t total = 0;
    uint8_t i;
    for (i = 0; i < dice; i++) {
        total += rng_range(1, sides);
    }
    return total;
}
