#include "core/vendor/vendor_perf.h"
#include <math.h>
#include <string.h>

#ifndef ROGUE_VENDOR_PERF_MAX
#define ROGUE_VENDOR_PERF_MAX 64
#endif

/* SoA arrays */
static float g_demand[ROGUE_VENDOR_PERF_MAX];
static float g_scarcity[ROGUE_VENDOR_PERF_MAX];
static int g_last_refresh_tick[ROGUE_VENDOR_PERF_MAX];
static int g_vendor_count = 0;
static int g_slice_size = 8; /* default */
static int g_next_index = 0; /* scheduler cursor */

void rogue_vendor_perf_reset(void)
{
    memset(g_demand, 0, sizeof g_demand);
    memset(g_scarcity, 0, sizeof g_scarcity);
    memset(g_last_refresh_tick, 0xFF, sizeof g_last_refresh_tick);
    g_vendor_count = 0;
    g_slice_size = 8;
    g_next_index = 0;
}

void rogue_vendor_perf_init(int vendor_count)
{
    if (vendor_count < 0)
        vendor_count = 0;
    if (vendor_count > ROGUE_VENDOR_PERF_MAX)
        vendor_count = ROGUE_VENDOR_PERF_MAX;
    g_vendor_count = vendor_count;
    for (int i = 0; i < g_vendor_count; i++)
    {
        g_demand[i] = 0.0f;
        g_scarcity[i] = 0.0f;
        g_last_refresh_tick[i] = -1;
    }
}

int rogue_vendor_perf_vendor_count(void) { return g_vendor_count; }
size_t rogue_vendor_perf_memory_bytes(void)
{
    return sizeof(float) * 2 * (size_t) g_vendor_count + sizeof(int) * (size_t) g_vendor_count;
}

static void note_bounds(int idx)
{
    if (idx < 0 || idx >= g_vendor_count)
        return;
}

void rogue_vendor_perf_note_sale(int idx)
{
    if (idx < 0 || idx >= g_vendor_count)
        return;
    g_demand[idx] = g_demand[idx] * 0.8f + 1.0f;
    g_scarcity[idx] = g_scarcity[idx] * 0.98f + 0.5f;
}
void rogue_vendor_perf_note_buyback(int idx)
{
    if (idx < 0 || idx >= g_vendor_count)
        return;
    g_demand[idx] = g_demand[idx] * 0.8f - 1.0f;
    g_scarcity[idx] = g_scarcity[idx] * 0.98f - 0.5f;
}
float rogue_vendor_perf_demand_score(int idx)
{
    if (idx < 0 || idx >= g_vendor_count)
        return 0.0f;
    return g_demand[idx];
}
float rogue_vendor_perf_scarcity_score(int idx)
{
    if (idx < 0 || idx >= g_vendor_count)
        return 0.0f;
    return g_scarcity[idx];
}

void rogue_vendor_perf_scheduler_config(int slice_size)
{
    if (slice_size <= 0)
        slice_size = 8;
    g_slice_size = slice_size;
}

int rogue_vendor_perf_scheduler_tick(int tick_id)
{
    if (g_vendor_count == 0)
        return 0;
    int processed = 0;
    int slice = g_slice_size;
    if (slice > g_vendor_count)
        slice = g_vendor_count;
    for (int i = 0; i < slice; i++)
    {
        int idx = g_next_index++;
        if (g_next_index >= g_vendor_count)
            g_next_index = 0;
        g_last_refresh_tick[idx] = tick_id;
        processed++;
    }
    return processed;
}

int rogue_vendor_perf_last_refresh_tick(int idx)
{
    if (idx < 0 || idx >= g_vendor_count)
        return -1;
    return g_last_refresh_tick[idx];
}
