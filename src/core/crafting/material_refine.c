#include "core/crafting/material_refine.h"
#include "core/crafting/material_registry.h"
#include "core/crafting/rng_streams.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ROGUE_MATERIAL_REGISTRY_CAP
#define ROGUE_MATERIAL_REGISTRY_CAP 128
#endif

/**
 * @file material_refine.c
 * @brief Material refinement ledger and refinement algorithm.
 *
 * This module maintains a per-material quality ledger (counts of materials
 * at each quality level) and implements refinement logic that consumes
 * source-quality material and produces higher-quality outputs with
 * failure/critical mechanics.
 */

/**
 * @brief Quality ledger: per material (registry index) we store counts for
 * qualities 0..ROGUE_MATERIAL_QUALITY_MAX inclusive.
 *
 * The first dimension is limited to ROGUE_MATERIAL_REGISTRY_CAP to avoid
 * unbounded allocations; out-of-range material indices are treated as
 * invalid by helpers below.
 */
static int g_quality[ROGUE_MATERIAL_REGISTRY_CAP][ROGUE_MATERIAL_QUALITY_MAX + 1];

/**
 * @brief Zero all quality counts for all materials.
 *
 * This resets the internal ledger to an empty state.
 */
void rogue_material_quality_reset(void) { memset(g_quality, 0, sizeof g_quality); }

/**
 * @brief Validate a material registry index.
 *
 * @param m Material registry index.
 * @return Non-zero when valid, zero otherwise.
 */
static int valid_material(int m) { return m >= 0 && m < ROGUE_MATERIAL_REGISTRY_CAP; }

/**
 * @brief Add some quantity of a material at a specific quality into the ledger.
 *
 * This function clamps the quality into [0, ROGUE_MATERIAL_QUALITY_MAX] and
 * increments the internal counter by count.
 *
 * @param material_def Material registry index.
 * @param quality Quality level to add (clamped).
 * @param count Number of units to add (must be >= 0).
 * @return 0 on success, -1 on invalid input.
 */
int rogue_material_quality_add(int material_def, int quality, int count)
{
    if (!valid_material(material_def) || count < 0)
        return -1;
    if (quality < 0)
        quality = 0;
    if (quality > ROGUE_MATERIAL_QUALITY_MAX)
        quality = ROGUE_MATERIAL_QUALITY_MAX;
    g_quality[material_def][quality] += count;
    return 0;
}

/**
 * @brief Consume count units of a material at given quality from the ledger.
 *
 * @param material_def Material registry index.
 * @param quality Quality level to consume.
 * @param count Number of units to consume (must be > 0).
 * @return Number of units consumed on success, -1 on invalid input, -2 if
 *         insufficient quantity is available.
 */
int rogue_material_quality_consume(int material_def, int quality, int count)
{
    if (!valid_material(material_def) || count <= 0)
        return -1;
    if (quality < 0 || quality > ROGUE_MATERIAL_QUALITY_MAX)
        return -1;
    int have = g_quality[material_def][quality];
    if (have < count)
        return -2;
    g_quality[material_def][quality] -= count;
    return count;
}

/**
 * @brief Query the count of a material at a specific quality.
 *
 * @param material_def Material registry index.
 * @param quality Quality level to query.
 * @return Count at quality, or -1 on invalid input.
 */
int rogue_material_quality_count(int material_def, int quality)
{
    if (!valid_material(material_def) || quality < 0 || quality > ROGUE_MATERIAL_QUALITY_MAX)
        return -1;
    return g_quality[material_def][quality];
}

/**
 * @brief Return total count of all qualities for the material.
 *
 * @param material_def Material registry index.
 * @return Total count or -1 on invalid material index.
 */
int rogue_material_quality_total(int material_def)
{
    if (!valid_material(material_def))
        return -1;
    int sum = 0;
    for (int q = 0; q <= ROGUE_MATERIAL_QUALITY_MAX; q++)
        sum += g_quality[material_def][q];
    return sum;
}

/**
 * @brief Compute weighted average quality for a material.
 *
 * Returns the integer floor of the average quality computed as
 * sum(q * count_q) / sum(count_q). If no items exist, returns -1.
 *
 * @param material_def Material registry index.
 * @return Average quality (0..ROGUE_MATERIAL_QUALITY_MAX) or -1 if no data.
 */
int rogue_material_quality_average(int material_def)
{
    if (!valid_material(material_def))
        return -1;
    long long num = 0;
    long long denom = 0;
    for (int q = 0; q <= ROGUE_MATERIAL_QUALITY_MAX; q++)
    {
        int c = g_quality[material_def][q];
        if (c > 0)
        {
            num += (long long) q * c;
            denom += c;
        }
    }
    if (denom == 0)
        return -1;
    return (int) (num / denom);
}

/**
 * @brief Bias metric: normalized average quality in [0.0, 1.0].
 *
 * If no items exist the function returns 0.0f.
 *
 * @param material_def Material registry index.
 * @return Normalized average quality as a float.
 */
float rogue_material_quality_bias(int material_def)
{
    int avg = rogue_material_quality_average(material_def);
    if (avg < 0)
        return 0.0f;
    return (float) avg / (float) ROGUE_MATERIAL_QUALITY_MAX;
}

/* RNG now sourced from deterministic refinement stream if state pointer NULL */
/**
 * @brief A tiny legacy LCG used when a caller provides an RNG state pointer.
 *
 * This updates the provided state in-place and returns the new state.
 * The algorithm matches the simple 32-bit LCG used historically in the
 * refinement logic.
 */
static unsigned int legacy_lcg_step(unsigned int* s)
{
    *s = (*s * 1664525u) + 1013904223u;
    return *s;
}

/**
 * @brief Attempt to refine material from one quality to another.
 *
 * The algorithm consumes consume_count units of material at from_quality
 * and attempts to produce higher-quality units at to_quality. The base
 * production is 70% of consumed units; the refinement has failure and
 * critical outcomes:
 *  - 10% failure: produced reduced to 25% of base (may result in total loss)
 *  - 5% critical (rolls 10-14): produced +50% (rounded), and a further
 *    overflow (20% of produced) may escalate into the next higher quality
 *    (to_quality+1) if available
 *
 * The function updates the internal ledger accordingly.
 *
 * Errors:
 *  - -1 : invalid parameters or rng_state/out pointers missing
 *  - -2 : insufficient source material available at from_quality
 *  - -3 : refinement produced nothing (all lost on failure)
 *
 * @param material_def Material registry index to refine.
 * @param from_quality Source quality level to consume.
 * @param to_quality Destination quality level to add produced units.
 * @param consume_count Number of source units to consume (>0).
 * @param rng_state Optional pointer to a caller-provided RNG state. If
 *                  NULL, the module's deterministic refinement RNG stream
 *                  is used instead.
 * @param out_produced Out: number of produced units (set on success).
 * @param out_crit Out: flag set to 1 when a critical occurred, otherwise 0.
 * @return 0 on success, negative error code on failure.
 */
int rogue_material_refine(int material_def, int from_quality, int to_quality, int consume_count,
                          unsigned int* rng_state, int* out_produced, int* out_crit)
{
    if (out_produced)
        *out_produced = 0;
    if (out_crit)
        *out_crit = 0;
    if (!valid_material(material_def) || !rng_state || !out_produced || !out_crit)
        return -1;
    if (from_quality < 0 || from_quality > ROGUE_MATERIAL_QUALITY_MAX || to_quality < 0 ||
        to_quality > ROGUE_MATERIAL_QUALITY_MAX || to_quality <= from_quality || consume_count <= 0)
        return -1;
    int have = rogue_material_quality_count(material_def, from_quality);
    if (have < consume_count)
        return -2;
    /* Consume source up front */
    g_quality[material_def][from_quality] -= consume_count;
    /* Base production 70% */
    int produced = (int) ((long long) consume_count * 70ll / 100ll);
    unsigned int r;
    if (rng_state)
    {
        r = legacy_lcg_step(rng_state);
    }
    else
    {
        r = rogue_rng_next(ROGUE_RNG_STREAM_REFINEMENT);
    }
    unsigned int roll = r % 100u; /* 0..99 */
    if (roll < 10u)
    {                                                           /* failure 10% */
        produced = (int) ((long long) produced * 25ll / 100ll); /* 25% of base */
        if (produced <= 0)
        { /* all lost */
            if (out_produced)
                *out_produced = 0;
            return -3;
        }
    }
    else if (roll >= 10u && roll < 15u)
    {                                                               /* critical 5% (10-14) */
        produced = (int) (produced + (produced + 1) / 2);           /* +50% rounded */
        int overflow = (int) ((long long) produced * 20ll / 100ll); /* 20% escalates */
        if (overflow > 0 && to_quality < ROGUE_MATERIAL_QUALITY_MAX)
        {
            g_quality[material_def][to_quality + 1] += overflow;
            produced -= overflow;
            if (out_crit)
                *out_crit = 1;
        }
        else if (out_crit)
            *out_crit = 1;
    }
    if (produced > 0)
        g_quality[material_def][to_quality] += produced;
    else
        return -3;
    if (out_produced)
        *out_produced = produced;
    return 0;
}
