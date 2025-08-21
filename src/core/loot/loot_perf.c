#include "core/loot/loot_perf.h"
#include <string.h>
#include <time.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h> /* LARGE_INTEGER, QueryPerformanceCounter */
#endif
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
#include <emmintrin.h>
#endif

/* Simple fixed pool for affix roll scratch weights (Phase 17.1 object pooling) */
#define ROGUE_LOOT_WEIGHT_SCRATCH_CAP 32
#define ROGUE_LOOT_WEIGHT_SCRATCH_SIZE 256

typedef struct WeightScratch
{
    int in_use;
    int count;
    int weights[ROGUE_LOOT_WEIGHT_SCRATCH_SIZE];
} WeightScratch;

static WeightScratch g_weight_scratch[ROGUE_LOOT_WEIGHT_SCRATCH_CAP];
static int g_weight_scratch_in_use = 0;
static int g_weight_scratch_max_in_use = 0;

static struct
{
    RogueLootPerfMetrics m;
    unsigned long long affix_roll_t0;
} g_perf;

/* Cross-platform monotonic time (nanoseconds). Uses clock_gettime if available, else clock()
 * fallback (low resolution). */
static unsigned long long rogue_time_now_ns(void)
{
#if defined(_WIN32) && (defined(_MSC_VER))
    static int inited = 0;
    static double freq_inv = 0.0;
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!inited)
    {
        QueryPerformanceFrequency(&f);
        freq_inv = 1000000000.0 / (double) f.QuadPart;
        inited = 1;
    }
    QueryPerformanceCounter(&c);
    return (unsigned long long) ((double) c.QuadPart * freq_inv);
#elif defined(CLOCK_MONOTONIC_RAW)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (unsigned long long) ts.tv_sec * 1000000000ull + (unsigned long long) ts.tv_nsec;
#elif defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long) ts.tv_sec * 1000000000ull + (unsigned long long) ts.tv_nsec;
#else
    return (unsigned long long) clock() * (1000000000ull / (unsigned long long) CLOCKS_PER_SEC);
#endif
}

void* rogue_loot_weight_scratch_acquire(void)
{
    for (int i = 0; i < ROGUE_LOOT_WEIGHT_SCRATCH_CAP; i++)
        if (!g_weight_scratch[i].in_use)
        {
            g_weight_scratch[i].in_use = 1;
            g_weight_scratch_in_use++;
            if (g_weight_scratch_in_use > g_weight_scratch_max_in_use)
                g_weight_scratch_max_in_use = g_weight_scratch_in_use;
            g_perf.m.affix_pool_acquires++;
            return &g_weight_scratch[i];
        }
    return NULL;
}
void rogue_loot_weight_scratch_release(void* p)
{
    if (!p)
        return;
    WeightScratch* ws = (WeightScratch*) p;
    if (ws->in_use)
    {
        ws->in_use = 0;
        g_weight_scratch_in_use--;
        g_perf.m.affix_pool_releases++;
    }
}

static int sum_weights_scalar(const int* w, int n)
{
    unsigned long long t0 = rogue_time_now_ns();
    int total = 0;
    for (int i = 0; i < n; i++)
    {
        total += w[i];
    }
    g_perf.m.affix_roll_scalar_sums++;
    g_perf.m.weight_sum_time_ns += (rogue_time_now_ns() - t0);
    return total;
}

static int sum_weights_simd(const int* w, int n)
{
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
    unsigned long long t0 = rogue_time_now_ns();
    int i = 0;
    __m128i acc = _mm_setzero_si128();
    for (; i + 4 <= n; i += 4)
    {
        __m128i v = _mm_loadu_si128((const __m128i*) (w + i));
        acc = _mm_add_epi32(acc, v);
    }
    int buf[4];
    _mm_storeu_si128((__m128i*) buf, acc);
    int total = buf[0] + buf[1] + buf[2] + buf[3];
    for (; i < n; i++)
        total += w[i];
    g_perf.m.affix_roll_simd_sums++;
    g_perf.m.weight_sum_time_ns += (rogue_time_now_ns() - t0);
    return total;
#else
    return sum_weights_scalar(w, n);
#endif
}

int rogue_loot_perf_simd_enabled(void)
{
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
    return 1;
#else
    return 0;
#endif
}

void rogue_loot_perf_reset(void)
{
    memset(&g_perf, 0, sizeof g_perf);
    g_weight_scratch_in_use = 0;
    g_weight_scratch_max_in_use = 0;
}
void rogue_loot_perf_get(RogueLootPerfMetrics* out)
{
    if (out)
    {
        *out = g_perf.m;
        out->affix_pool_max_in_use = (uint32_t) g_weight_scratch_max_in_use;
    }
}

void rogue_loot_perf_affix_roll_begin(void) { g_perf.affix_roll_t0 = rogue_time_now_ns(); }
void rogue_loot_perf_affix_roll_end(void)
{
    if (g_perf.affix_roll_t0)
    {
        unsigned long long dt = rogue_time_now_ns() - g_perf.affix_roll_t0;
        g_perf.m.affix_roll_time_ns += dt;
        g_perf.affix_roll_t0 = 0;
    }
}

/* Test helper performs dummy weighted rolls using pool + SIMD sum if available */
int rogue_loot_perf_test_rolls(int loops)
{
    int success = 0;
    for (int i = 0; i < loops; i++)
    {
        WeightScratch* ws = (WeightScratch*) rogue_loot_weight_scratch_acquire();
        if (!ws)
            break;
        ws->count = 16;
        for (int k = 0; k < ws->count; k++)
        {
            ws->weights[k] = (k + 3);
        }
        int total = rogue_loot_perf_simd_enabled() ? sum_weights_simd(ws->weights, ws->count)
                                                   : sum_weights_scalar(ws->weights, ws->count);
        g_perf.m.affix_roll_total_weights += (uint32_t) total;
        g_perf.m.affix_roll_calls++;
        if (total > 0)
            success++;
        rogue_loot_weight_scratch_release(ws);
    }
    return success;
}
