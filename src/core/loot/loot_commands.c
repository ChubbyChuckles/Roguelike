/**
 * @file loot_commands.c
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 *
 * This file implements a simple console command parser for loot system debugging and control.
 * Supports commands like 'weight <rarity> <factor>' to set dynamic factors,
 * 'reset_dyn'/'reset_stats' to clear state, 'stats' to snapshot rarity counts, 'get <rarity>' to
 * query factors. Parses input line, trims whitespace, lowercases cmd, handles args with sscanf
 * (platform-safe), outputs status/ errors to buffer. Returns 0 success, 1 empty/null, 2
 * usage/unknown.
 */

#include "loot_commands.h"
#include "loot_dynamic_weights.h"
#include "loot_stats.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Internal formatted write to output buffer using varargs.
 *
 * Wrapper for vsnprintf with null-check and size guard.
 *
 * @param out Output buffer.
 * @param out_sz Size of output buffer.
 * @param fmt Format string.
 * @param ... Varargs for format.
 */
static void wr(char* out, int out_sz, const char* fmt, ...)
{
    if (!out || out_sz <= 0)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out, out_sz, fmt, ap);
    va_end(ap);
}

/**
 * @brief Executes a loot command from input line, outputs result/error to buffer.
 *
 * Trims leading whitespace, parses first token as lowercase cmd, advances to args. Handles:
 * - weight <rarity 0-4> <factor>: sets dynamic factor, confirms with current value.
 * - reset_dyn: resets dynamic weights to 1.0.
 * - reset_stats: resets loot stats.
 * - stats: snapshots and formats rarity counts (C/U/R/E/L).
 * - get <rarity 0-4>: queries current dynamic factor.
 * Uses platform-safe sscanf (sscanf_s on MSVC). Returns 0 success, 1 null/empty, 2
 * usage/unknown/invalid.
 *
 * @param line Null-terminated input command line.
 * @param out Output buffer for status/error messages.
 * @param out_sz Size of output buffer.
 * @return Execution status: 0 OK, 1 empty input, 2 error (usage/range/unknown).
 */
int rogue_loot_run_command(const char* line, char* out, int out_sz)
{
    if (!line)
    {
        wr(out, out_sz, "ERR: null line");
        return 1;
    }
    while (isspace((unsigned char) *line))
        line++;
    if (!*line)
    {
        wr(out, out_sz, "ERR: empty");
        return 1;
    }
    char cmd[32];
    int consumed = 0;
#if defined(_MSC_VER)
    if (sscanf_s(line, "%31s%n", cmd, (unsigned) sizeof(cmd), &consumed) != 1)
    {
        wr(out, out_sz, "ERR: token");
        return 1;
    }
#else
    if (sscanf(line, "%31s%n", cmd, &consumed) != 1)
    {
        wr(out, out_sz, "ERR: token");
        return 1;
    }
#endif
    for (char* p = cmd; *p; ++p)
        *p = (char) tolower((unsigned char) *p);
    const char* args = line + consumed;
    while (isspace((unsigned char) *args))
        args++;
    if (strcmp(cmd, "weight") == 0)
    {
        int rarity;
        float factor;
#if defined(_MSC_VER)
        if (sscanf_s(args, "%d %f", &rarity, &factor) != 2)
        {
            wr(out, out_sz, "ERR: usage weight <rarity 0-4> <factor>");
            return 2;
        }
#else
        if (sscanf(args, "%d %f", &rarity, &factor) != 2)
        {
            wr(out, out_sz, "ERR: usage weight <rarity 0-4> <factor>");
            return 2;
        }
#endif
        if (rarity < 0 || rarity > 4)
        {
            wr(out, out_sz, "ERR: rarity range");
            return 2;
        }
        rogue_loot_dyn_set_factor(rarity, factor);
        wr(out, out_sz, "OK: weight r%d=%.3f", rarity, rogue_loot_dyn_get_factor(rarity));
        return 0;
    }
    else if (strcmp(cmd, "reset_dyn") == 0)
    {
        rogue_loot_dyn_reset();
        wr(out, out_sz, "OK: dyn reset");
        return 0;
    }
    else if (strcmp(cmd, "reset_stats") == 0)
    {
        rogue_loot_stats_reset();
        wr(out, out_sz, "OK: stats reset");
        return 0;
    }
    else if (strcmp(cmd, "stats") == 0)
    {
        int counts[5];
        rogue_loot_stats_snapshot(counts);
        wr(out, out_sz, "STATS: C=%d U=%d R=%d E=%d L=%d", counts[0], counts[1], counts[2],
           counts[3], counts[4]);
        return 0;
    }
    else if (strcmp(cmd, "get") == 0)
    {
        int rarity;
        if (
#if defined(_MSC_VER)
            sscanf_s(args, "%d", &rarity)
#else
            sscanf(args, "%d", &rarity)
#endif
                != 1 ||
            rarity < 0 || rarity > 4)
        {
            wr(out, out_sz, "ERR: usage get <rarity 0-4>");
            return 2;
        }
        wr(out, out_sz, "FACTOR: r%d=%.3f", rarity, rogue_loot_dyn_get_factor(rarity));
        return 0;
    }
    wr(out, out_sz, "ERR: unknown cmd");
    return 2;
}
