/* fx_internal.h - Internal helpers for audio/vfx module (not for public include) */
#ifndef ROGUE_FX_INTERNAL_H
#define ROGUE_FX_INTERNAL_H

#include <stdint.h>

/* Shared deterministic RNG for FX systems */
uint32_t rogue_fx_rand_u32(void);
float rogue_fx_rand01(void);
float rogue_fx_rand_normal01(void);

#endif /* ROGUE_FX_INTERNAL_H */
