/* fx_rng.c - Deterministic RNG helpers for FX systems */
#include "effects.h"     /* for public rogue_fx_debug_set_seed */
#include "fx_internal.h" /* for internal RNG helpers used by other fx modules */
#include <math.h>

static uint32_t g_fx_seed = 0xA5F0C3D2u;

void rogue_fx_debug_set_seed(uint32_t seed) { g_fx_seed = seed ? seed : 0xA5F0C3D2u; }

uint32_t rogue_fx_rand_u32(void)
{
    uint32_t x = g_fx_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_fx_seed = x ? x : 0xA5F0C3D2u;
    return g_fx_seed;
}

float rogue_fx_rand01(void)
{
    return (rogue_fx_rand_u32() & 0xFFFFFFu) / 16777216.0f; /* 24-bit mantissa */
}

float rogue_fx_rand_normal01(void)
{
    /* Box-Muller with simple clamping to [-4,4] */
    float u1 = rogue_fx_rand01();
    float u2 = rogue_fx_rand01();
    if (u1 < 1e-7f)
        u1 = 1e-7f;
    const double two_pi = 6.283185307179586;
    float z = (float) sqrt(-2.0 * log((double) u1)) * (float) cos(two_pi * u2);
    if (z < -4.0f)
        z = -4.0f;
    if (z > 4.0f)
        z = 4.0f;
    return z;
}
