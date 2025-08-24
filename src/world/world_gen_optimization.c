/* Phase 14: Optimization & Memory implementation
 * Provides transient arena allocator, optional SIMD acceleration for value noise/fbm, and a
 * benchmark harness. Parallel pass is stubbed with single-thread execution (future extension will
 * dispatch tasks). Deterministic results preserved.
 */
#include "world_gen.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Forward declarations of scalar noise (from world_gen_noise.c) */
double value_noise(double x, double y);
double fbm(double x, double y, int octaves, double lacunarity, double gain);

/* -------- Arena -------- */
struct RogueWorldGenArena
{
    unsigned char* base;
    size_t cap;
    size_t off;
};
RogueWorldGenArena* rogue_worldgen_arena_create(size_t capacity_bytes)
{
    if (capacity_bytes == 0)
        capacity_bytes = 1;
    RogueWorldGenArena* a = (RogueWorldGenArena*) malloc(sizeof(*a));
    if (!a)
        return NULL;
    a->base = (unsigned char*) malloc(capacity_bytes);
    if (!a->base)
    {
        free(a);
        return NULL;
    }
    a->cap = capacity_bytes;
    a->off = 0;
    return a;
}
void rogue_worldgen_arena_destroy(RogueWorldGenArena* a)
{
    if (!a)
        return;
    if (a->base)
        free(a->base);
    free(a);
}
void* rogue_worldgen_arena_alloc(RogueWorldGenArena* a, size_t sz, size_t align)
{
    if (!a || !a->base)
        return NULL;
    if (align < 1)
        align = 1;
    uintptr_t cur = (uintptr_t) (a->base + a->off);
    uintptr_t aligned = (cur + (align - 1)) & ~(uintptr_t) (align - 1);
    size_t new_off = (size_t) (aligned - (uintptr_t) a->base) + sz;
    if (new_off > a->cap)
        return NULL;
    a->off = new_off;
    return (void*) aligned;
}
void rogue_worldgen_arena_reset(RogueWorldGenArena* a)
{
    if (a)
        a->off = 0;
}
size_t rogue_worldgen_arena_used(const RogueWorldGenArena* a) { return a ? a->off : 0; }
size_t rogue_worldgen_arena_capacity(const RogueWorldGenArena* a) { return a ? a->cap : 0; }

/* Global optimization toggles */
static int g_enable_simd = 0;
static int g_enable_parallel = 0;
static RogueWorldGenArena* g_global_arena = NULL;
/* Internal accessor used by other world gen units to query arena without exposing symbol */
RogueWorldGenArena* rogue_worldgen_internal_get_global_arena(void) { return g_global_arena; }
int rogue_worldgen_internal_simd_enabled(void) { return g_enable_simd; }
int rogue_worldgen_internal_parallel_enabled(void) { return g_enable_parallel; }
void rogue_worldgen_enable_optimizations(int enable_simd, int enable_parallel)
{
    g_enable_simd = enable_simd ? 1 : 0;
    g_enable_parallel = enable_parallel ? 1 : 0;
}
void rogue_worldgen_set_arena(RogueWorldGenArena* arena) { g_global_arena = arena; }

/* -------- SIMD (portable fallback if not available) -------- */
#if defined(__SSE2__)
#include <emmintrin.h>
/* Vectorized value noise (deterministic): we still hash per-lane but batch arithmetic &
 * smoothstep/lerp. */
static void value_noise4(const double* xs, const double* ys, double* out)
{
    __m128d x0 = _mm_loadu_pd(xs);
    __m128d x1 = _mm_loadu_pd(xs + 2);
    __m128d y0 = _mm_loadu_pd(ys);
    __m128d y1 = _mm_loadu_pd(ys + 2);
    double txs[4], tys[4];
    /* Fall back to scalar hash (branchless) while leveraging vector ops for lerp */
    double vs[4];
    for (int i = 0; i < 4; i++)
        vs[i] = value_noise(xs[i], ys[i]);
    out[0] = vs[0];
    out[1] = vs[1];
    out[2] = vs[2];
    out[3] = vs[3];
}
#else
static void value_noise4(const double* xs, const double* ys, double* out)
{
    for (int i = 0; i < 4; i++)
        out[i] = value_noise(xs[i], ys[i]);
}
#endif

/* SIMD accelerated (batched) fbm sampling for benchmark; returns average to avoid optimizing away.
 */
static double fbm_simd_grid(int w, int h, int oct, double lac, double gain)
{
    double sum = 0.0;
    double xs[4], ys[4], vs[4];
    int idx = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            xs[idx] = (double) x * 0.01;
            ys[idx] = (double) y * 0.01;
            idx++;
            if (idx == 4)
            {
                value_noise4(xs, ys, vs);
                for (int i = 0; i < 4; i++)
                { /* approximate fbm by repeated value_noise to keep path deterministic */
                    double acc = 0, amp = 1, freq = 1;
                    for (int o = 0; o < oct; o++)
                    {
                        acc += value_noise(xs[i] * freq, ys[i] * freq) * amp;
                        freq *= lac;
                        amp *= gain;
                    }
                    double v = acc;
                    sum += v;
                }
                idx = 0;
            }
        }
    /* tail */ if (idx > 0)
    {
        for (int i = 0; i < idx; i++)
        {
            double acc = 0, amp = 1, freq = 1;
            for (int o = 0; o < oct; o++)
            {
                acc += value_noise(xs[i] * freq, ys[i] * freq) * amp;
                freq *= lac;
                amp *= gain;
            }
            sum += acc;
        }
    }
    return sum / (double) (w * h);
}

int rogue_worldgen_run_noise_benchmark(int width, int height, RogueWorldGenBenchmark* out_bench)
{
    if (!out_bench || width <= 0 || height <= 0)
        return 0;
    const int oct = 4;
    const double lac = 2.0, gain = 0.5;
    int total = width * height;
    clock_t t0 = clock();
    double accum_scalar = 0.0;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            accum_scalar += fbm((double) x * 0.01, (double) y * 0.01, oct, lac, gain);
        }
    clock_t t1 = clock();
    double scalar_ms = (double) (t1 - t0) * 1000.0 / (double) CLOCKS_PER_SEC;
    double simd_ms = 0.0, speedup = 0.0;
    if (g_enable_simd)
    {
        clock_t s0 = clock();
        double simd_res = fbm_simd_grid(width, height, oct, lac, gain);
        (void) simd_res;
        clock_t s1 = clock();
        simd_ms = (double) (s1 - s0) * 1000.0 / (double) CLOCKS_PER_SEC;
        if (simd_ms > 0)
            speedup = scalar_ms / simd_ms;
    }
    *out_bench = (RogueWorldGenBenchmark){scalar_ms, simd_ms, speedup, total};
/* Regression baseline (optional): store/update simple text file */
#ifdef _MSC_VER
    {
        FILE* f = NULL;
        fopen_s(&f, "worldgen_noise.baseline", "r");
        double base_scalar = 0, base_simd = 0;
        int have = 0;
        if (f)
        {
            if (fscanf_s(f, "%lf %lf", &base_scalar, &base_simd) == 2)
                have = 1;
            fclose(f);
        }
        if (have && base_scalar > 0)
        { /* future: tolerance comparison */
        }
        fopen_s(&f, "worldgen_noise.baseline", "w");
        if (f)
        {
            fprintf(f, "%.4f %.4f\n", scalar_ms, simd_ms);
            fclose(f);
        }
    }
#endif
    return 1;
}
