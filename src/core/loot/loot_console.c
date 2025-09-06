/**
 * @file loot_console.c
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 *
 * This file provides console-friendly output and telemetry export for loot statistics.
 * Includes formatted histogram of rarity counts (COMMON:count\n ... TOTAL:sum\n) and JSON export
 * of session data (timestamp, rarity_counts array, dynamic_factors array, window_size).
 * Relies on rarity_names array for labels (COMMON,UNCOMMON,RARE,EPIC,LEGENDARY).
 */

#include "loot_console.h"
#include "loot_dynamic_weights.h"
#include "loot_rarity.h"
#include "loot_stats.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ROGUE_RARITY_MAX 5
static const char* rarity_names[ROGUE_RARITY_MAX] = {"COMMON", "UNCOMMON", "RARE", "EPIC",
                                                     "LEGENDARY"};

/**
 * @brief Formats a histogram of rarity counts into a string buffer.
 *
 * Appends "NAME:count\n" for each rarity using rarity_names, followed by "TOTAL:sum\n".
 * Truncates/null-terminates if buffer overflows.
 *
 * @param out Output buffer.
 * @param out_sz Size of output buffer.
 * @return Bytes written, or -1 if invalid out/sz.
 */
int rogue_loot_histogram_format(char* out, int out_sz)
{
    if (!out || out_sz <= 0)
        return -1;
    int counts[ROGUE_RARITY_MAX];
    rogue_loot_stats_snapshot(counts);
    int written = 0;
    for (int i = 0; i < ROGUE_RARITY_MAX; i++)
    {
        int c = counts[i];
        int n = snprintf(out + written, (size_t) (out_sz - written), "%s:%d\n", rarity_names[i], c);
        if (n < 0)
            return -1;
        if (n >= out_sz - written)
        {
            if (out_sz > 0)
                out[out_sz - 1] = '\0';
            return written;
        }
        written += n;
    }
    int total = 0;
    for (int i = 0; i < ROGUE_RARITY_MAX; i++)
        total += counts[i];
    if (out_sz - written > 0)
        snprintf(out + written, (size_t) (out_sz - written), "TOTAL:%d\n", total);
    return written;
}

/**
 * @brief Exports loot telemetry (counts, factors, timestamp, window) to JSON file.
 *
 * Writes to path in wb mode (MSVC fopen_s), JSON format with timestamp (time(NULL)), rarity_counts
 * array, dynamic_factors array (%.3f), window_size constant. Returns 0 success, -1 null path, -2
 * open fail.
 *
 * @param path Path to output JSON file.
 * @return 0 on success, -1 if null path, -2 if file open failed.
 */
int rogue_loot_export_telemetry(const char* path)
{
    if (!path)
        return -1;
    FILE* f = NULL;
    if (fopen_s(&f, path, "wb") != 0 || !f)
        return -2;
    int counts[ROGUE_RARITY_MAX];
    rogue_loot_stats_snapshot(counts);
    double factors[ROGUE_RARITY_MAX];
    for (int i = 0; i < ROGUE_RARITY_MAX; i++)
        factors[i] = rogue_loot_dyn_get_factor(i);
    time_t t = time(NULL);
    fprintf(f, "{\n  \"timestamp\": %ld,\n  \"rarity_counts\": [", (long) t);
    for (int i = 0; i < ROGUE_RARITY_MAX; i++)
    {
        if (i)
            fputc(',', f);
        fprintf(f, "%d", counts[i]);
    }
    fputs("],\n  \"dynamic_factors\": [", f);
    for (int i = 0; i < ROGUE_RARITY_MAX; i++)
    {
        if (i)
            fputc(',', f);
        fprintf(f, "%.3f", factors[i]);
    }
    fprintf(f, "],\n  \"window_size\": %d\n}\n", ROGUE_LOOT_STATS_CAP);
    fclose(f);
    return 0;
}
