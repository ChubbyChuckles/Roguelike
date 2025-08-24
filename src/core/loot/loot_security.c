#include "loot_security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t fnv1a32(const void* data, size_t len, uint32_t h)
{
    const unsigned char* p = (const unsigned char*) data;
    if (h == 0)
        h = 2166136261u;
    for (size_t i = 0; i < len; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

uint32_t rogue_loot_roll_hash(int table_index, unsigned int seed_before, int drop_count,
                              const int* item_def_indices, const int* quantities,
                              const int* rarities)
{
    uint32_t h = 2166136261u;
    h = fnv1a32(&table_index, sizeof(table_index), h);
    h = fnv1a32(&seed_before, sizeof(seed_before), h);
    h = fnv1a32(&drop_count, sizeof(drop_count), h);
    for (int i = 0; i < drop_count; i++)
    {
        int id = item_def_indices ? item_def_indices[i] : -1;
        int qty = quantities ? quantities[i] : 0;
        int rar = rarities ? rarities[i] : -1;
        h = fnv1a32(&id, sizeof id, h);
        h = fnv1a32(&qty, sizeof qty, h);
        h = fnv1a32(&rar, sizeof rar, h);
    }
    return h;
}

static int g_obfuscation_enabled = 0;
void rogue_loot_security_enable_obfuscation(int enable) { g_obfuscation_enabled = enable ? 1 : 0; }
int rogue_loot_security_obfuscation_enabled(void) { return g_obfuscation_enabled; }

unsigned int rogue_loot_obfuscate_seed(unsigned int raw_seed, unsigned int salt)
{
    /* Simple reversible mix: xor, rotate, multiply, xor */
    unsigned int x = raw_seed ^ (salt * 0x9E3779B9u);
    x = (x << 13) | (x >> 19);
    x = x * 0x85EBCA6Bu + 0xC2B2AE35u;
    x ^= (x >> 16);
    return x;
}

static uint32_t g_last_files_hash = 0;
uint32_t rogue_loot_security_last_files_hash(void) { return g_last_files_hash; }
static int g_server_mode = 0;
void rogue_loot_security_set_server_mode(int enabled) { g_server_mode = enabled ? 1 : 0; }
int rogue_loot_security_server_mode(void) { return g_server_mode; }
int rogue_loot_server_verify(int table_index, unsigned int seed_before, int drop_count,
                             const int* item_def_indices, const int* quantities,
                             const int* rarities, uint32_t reported_hash)
{
    uint32_t h = rogue_loot_roll_hash(table_index, seed_before, drop_count, item_def_indices,
                                      quantities, rarities);
    return (h == reported_hash) ? 0 : 1;
}

/* Anomaly detector */
static int g_anom_window_cap = 128;
static float g_anom_baseline_high = 0.05f;  /* expected legendary+ fraction */
static float g_anom_spike_mult = 3.5f;      /* spike threshold multiplier */
static int g_anom_per_roll_high_thresh = 2; /* too many high rarity in single roll */
static int g_anom_flag = 0;
static int g_anom_counts_high = 0;
static int g_anom_samples = 0;
void rogue_loot_anomaly_reset(void)
{
    g_anom_flag = 0;
    g_anom_counts_high = 0;
    g_anom_samples = 0;
}
void rogue_loot_anomaly_config(int window_size, float baseline_high_frac, float spike_mult,
                               int per_roll_high_threshold)
{
    if (window_size > 8 && window_size <= 1024)
        g_anom_window_cap = window_size;
    if (baseline_high_frac > 0.0001f && baseline_high_frac < 0.5f)
        g_anom_baseline_high = baseline_high_frac;
    if (spike_mult >= 1.5f && spike_mult < 20.f)
        g_anom_spike_mult = spike_mult;
    if (per_roll_high_threshold >= 1 && per_roll_high_threshold < 32)
        g_anom_per_roll_high_thresh = per_roll_high_threshold;
}
void rogue_loot_anomaly_record(int drop_count, const int* rarities)
{
    if (!rarities || drop_count <= 0)
        return;
    int high = 0;
    for (int i = 0; i < drop_count; i++)
    {
        if (rarities[i] >= 4)
            high++;
    }
    if (high >= g_anom_per_roll_high_thresh)
        g_anom_flag = 1;
    g_anom_counts_high += high;
    g_anom_samples += drop_count;
    if (g_anom_samples > g_anom_window_cap)
    { /* decay oldest proportionally (approx) */
        float decay = 0.5f;
        g_anom_counts_high = (int) (g_anom_counts_high * decay);
        g_anom_samples = (int) (g_anom_samples * decay);
    }
    if (g_anom_samples > 0)
    {
        float frac = (float) g_anom_counts_high / (float) g_anom_samples;
        if (frac > g_anom_baseline_high * g_anom_spike_mult)
            g_anom_flag = 1;
    }
}
int rogue_loot_anomaly_flag(void) { return g_anom_flag; }

int rogue_loot_security_snapshot_files(const char* const* paths, int count)
{
    if (count < 0)
        return -1;
    uint32_t h = 2166136261u;
    char buf[512];
    for (int i = 0; i < count; i++)
    {
        const char* p = paths[i];
        if (!p)
            return -2;
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, p, "rb");
#else
        f = fopen(p, "rb");
#endif
        if (!f)
        { /* treat missing file as neutral (skip) so caller can still verify subset */
            continue;
        }
        size_t r;
        while ((r = fread(buf, 1, sizeof buf, f)) > 0)
        {
            h = fnv1a32(buf, r, h);
        }
        fclose(f);
    }
    g_last_files_hash = h;
    return 0;
}

int rogue_loot_security_verify_files(const char* const* paths, int count)
{
    if (count < 0)
        return -1;
    if (g_last_files_hash == 0)
        return -4;
    uint32_t h = 2166136261u;
    char buf[512];
    for (int i = 0; i < count; i++)
    {
        const char* p = paths[i];
        if (!p)
            return -2;
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, p, "rb");
#else
        f = fopen(p, "rb");
#endif
        if (!f)
            continue;
        size_t r;
        while ((r = fread(buf, 1, sizeof buf, f)) > 0)
        {
            h = fnv1a32(buf, r, h);
        }
        fclose(f);
    }
    if (g_last_files_hash == 0)
    {
        g_last_files_hash = h;
        return 0;
    }
    return (h == g_last_files_hash) ? 0 : 1;
}
