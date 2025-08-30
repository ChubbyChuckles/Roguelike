/**
 * @file world_gen_noise.c
 * @brief Provides noise and RNG utilities for world generation.
 * @details This module implements noise functions like value noise and FBM, along with a simple RNG
 * for deterministic world generation.
 */

/* Noise & RNG utilities for world generation */
#include "world_gen_internal.h"
#include <math.h>

/// @brief Global RNG state.
static unsigned int rng_state = 1u;

/**
 * @brief Seeds the RNG.
 * @param s The seed value.
 */
void rng_seed(unsigned int s)
{
    if (!s)
        s = 1;
    rng_state = s;
}

/**
 * @brief Generates a random unsigned 32-bit integer.
 * @return A random unsigned 32-bit integer.
 */
unsigned int rng_u32(void)
{
    unsigned int x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

/**
 * @brief Generates a random double in [0, 1).
 * @return A random double.
 */
double rng_norm(void) { return (double) rng_u32() / (double) 0xffffffffu; }

/**
 * @brief Generates a random integer in [lo, hi].
 * @param lo Lower bound.
 * @param hi Upper bound.
 * @return A random integer.
 */
int rng_range(int lo, int hi) { return lo + (int) (rng_norm() * (double) (hi - lo + 1)); }

/**
 * @brief Hashes two integers to a double in [0, 1).
 * @param x First integer.
 * @param y Second integer.
 * @return A hashed double.
 */
static double hash2(int x, int y)
{
    unsigned int h = (unsigned int) (x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return (double) (h & 0xffffff) / (double) 0xffffff;
}

/**
 * @brief Linear interpolation between two doubles.
 * @param a First value.
 * @param b Second value.
 * @param t Interpolation factor.
 * @return Interpolated value.
 */
static double lerp_d(double a, double b, double t) { return a + (b - a) * t; }

/**
 * @brief Smoothstep function for interpolation.
 * @param t Input value.
 * @return Smoothstepped value.
 */
static double smoothstep_d(double t) { return t * t * (3.0 - 2.0 * t); }

/**
 * @brief Computes value noise at a point.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return Noise value.
 */
double value_noise(double x, double y)
{
    int xi = (int) floor(x), yi = (int) floor(y);
    double tx = x - xi, ty = y - yi;
    double v00 = hash2(xi, yi), v10 = hash2(xi + 1, yi), v01 = hash2(xi, yi + 1),
           v11 = hash2(xi + 1, yi + 1);
    double sx = smoothstep_d(tx), sy = smoothstep_d(ty);
    double a = lerp_d(v00, v10, sx);
    double b = lerp_d(v01, v11, sx);
    return lerp_d(a, b, sy);
}

/**
 * @brief Computes fractal Brownian motion (FBM) noise.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param octaves Number of octaves.
 * @param lacunarity Lacunarity factor.
 * @param gain Gain factor.
 * @return FBM noise value.
 */
double fbm(double x, double y, int octaves, double lacunarity, double gain)
{
    double amp = 1.0, freq = 1.0, sum = 0.0, norm = 0.0;
    for (int i = 0; i < octaves; i++)
    {
        sum += value_noise(x * freq, y * freq) * amp;
        norm += amp;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum / (norm > 0 ? norm : 1.0);
}
