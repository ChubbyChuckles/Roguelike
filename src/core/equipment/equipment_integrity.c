/**
 * @file equipment_integrity.c
 * @brief Equipment system integrity validation and auditing
 *
 * Phase 15 Integrity helpers implementation providing comprehensive validation
 * of equipment system state and data integrity. Includes proc rate monitoring,
 * banned affix pair management, and equipment chain validation.
 *
 * This module provides three main integrity checking systems:
 * - Proc Rate Auditor (15.4): Monitors for anomalous proc trigger rates
 * - Banned Affix Pairs (15.5): Prevents invalid affix combinations
 * - Equip Chain & GUID Audits (15.6): Validates equipment state and uniqueness
 *
 * @note All validation functions return counts of detected issues
 * @note Auto-healing is not performed - issues are reported for external handling
 */

#include "equipment_integrity.h"
#include "../loot/loot_affixes.h"
#include "../loot/loot_instances.h"
#include "equipment.h"
#include "equipment_procs.h"

#include <string.h>

/* ---------------- Proc Rate Auditor (15.4) ---------------- */

/**
 * @brief Scan for anomalous proc trigger rates
 *
 * Monitors all active procs and identifies those exceeding the specified
 * triggers-per-minute threshold. Used to detect proc exploits or bugs
 * causing excessive proc activation.
 *
 * @param out Array to store detected anomalies (optional)
 * @param max_out Maximum number of anomalies to report
 * @param max_tpm Maximum allowed triggers per minute threshold
 * @return int Number of anomalous procs detected
 *
 * @note Anomalies are reported but not auto-corrected
 * @note Set max_out to 0 to count anomalies without storing details
 * @note Negative max_out values return 0 (no anomalies)
 */
int rogue_integrity_scan_proc_anomalies(RogueProcAnomaly* out, int max_out, float max_tpm)
{
    if (max_out < 0)
        return 0;
    int written = 0;
    int count = rogue_proc_count();
    for (int i = 0; i < count; i++)
    {
        float tpm = rogue_proc_triggers_per_min(i);
        if (tpm > max_tpm)
        {
            if (out && written < max_out)
            {
                out[written].proc_id = i;
                out[written].triggers_per_min = tpm;
            }
            written++;
        }
    }
    return written;
}

/* ---------------- Banned Affix Pairs (15.5) ---------------- */

#ifndef ROGUE_INTEGRITY_BANNED_PAIR_CAP
#define ROGUE_INTEGRITY_BANNED_PAIR_CAP 64
#endif

/**
 * @brief Internal structure for storing banned affix pairs
 */
typedef struct BannedPair
{
    int a, b; /**< Affix indices forming the banned pair */
} BannedPair;

/** @brief Global storage for banned affix pairs */
static BannedPair g_banned_pairs[ROGUE_INTEGRITY_BANNED_PAIR_CAP];
/** @brief Current count of registered banned pairs */
static int g_banned_pair_count = 0;

/**
 * @brief Clear all banned affix pair restrictions
 *
 * Removes all currently registered banned affix pairs, effectively
 * allowing all affix combinations.
 */
void rogue_integrity_clear_banned_affix_pairs(void) { g_banned_pair_count = 0; }

/**
 * @brief Add a banned affix pair restriction
 *
 * Registers a forbidden combination of two affixes that should not
 * appear together on the same item. The order of affixes doesn't matter
 * (a,b is treated the same as b,a).
 *
 * @param affix_a First affix index in the banned pair
 * @param affix_b Second affix index in the banned pair
 * @return int Operation result:
 *         - 0: Success - pair added
 *         - 1: Already exists - pair was already banned
 *         - -1: Invalid parameters (negative indices or identical affixes)
 *         - -2: Capacity exceeded - too many banned pairs
 */
int rogue_integrity_add_banned_affix_pair(int affix_a, int affix_b)
{
    if (affix_a < 0 || affix_b < 0 || affix_a == affix_b)
        return -1;
    if (g_banned_pair_count >= ROGUE_INTEGRITY_BANNED_PAIR_CAP)
        return -2;
    for (int i = 0; i < g_banned_pair_count; i++)
    {
        if ((g_banned_pairs[i].a == affix_a && g_banned_pairs[i].b == affix_b) ||
            (g_banned_pairs[i].a == affix_b && g_banned_pairs[i].b == affix_a))
            return 1;
    }
    g_banned_pairs[g_banned_pair_count].a = affix_a;
    g_banned_pairs[g_banned_pair_count].b = affix_b;
    g_banned_pair_count++;
    return 0;
}

/**
 * @brief Check if an item has banned affix combinations
 *
 * Validates whether the specified item instance contains any affix
 * combinations that have been registered as banned. Only checks
 * prefix-suffix combinations (ignores other affix types).
 *
 * @param inst_index Index of the item instance to check
 * @return int Check result:
 *         - 0: Item is allowed (no banned combinations)
 *         - 1: Item is banned (contains forbidden affix pair)
 *         - -1: Invalid item instance
 *
 * @note Only checks prefix and suffix affixes
 * @note Items without both prefix and suffix are considered allowed
 */
int rogue_integrity_is_item_banned(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    int a = it->prefix_index;
    int b = it->suffix_index;
    if (a < 0 || b < 0)
        return 0;
    for (int i = 0; i < g_banned_pair_count; i++)
    {
        int pa = g_banned_pairs[i].a, pb = g_banned_pairs[i].b;
        if ((a == pa && b == pb) || (a == pb && b == pa))
            return 1;
    }
    return 0;
}

/* ---------------- Equip Chain & GUID Audits (15.6) ---------------- */

/**
 * @brief 64-bit hash mixing function
 *
 * Combines two 64-bit values using a modified version of the MurmurHash
 * mixing function. Used for generating deterministic hashes of equipment
 * state transitions.
 *
 * @param h Current hash value
 * @param v Value to mix into the hash
 * @return unsigned long long Updated hash value
 */
static unsigned long long mix64(unsigned long long h, unsigned long long v)
{
    h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    return h;
}

/**
 * @brief Calculate expected equipment chain hash for an item
 *
 * Computes the expected hash value that should be stored in the item's
 * equip_hash_chain field based on its current equipment state. The hash
 * incorporates the item's GUID and all equipment slots where it is currently
 * equipped.
 *
 * @param inst_index Index of the item instance to hash
 * @return unsigned long long Expected hash value (0 if invalid item)
 *
 * @note Hash includes item GUID and equipment slot information
 * @note Used to detect equipment state corruption or tampering
 * @note Returns 0 for invalid item instances
 */
unsigned long long rogue_integrity_expected_item_equip_hash(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return 0ULL;
    unsigned long long h = 0ULL;
    for (int s = 0; s < ROGUE_EQUIP__COUNT; s++)
    {
        int cur = rogue_equip_get((enum RogueEquipSlot) s);
        if (cur == inst_index)
        {
            h = mix64(h, ((unsigned long long) s << 56) ^ it->guid ^ 0xE11AFBULL);
        }
    }
    return h;
}

/**
 * @brief Scan for equipment chain hash mismatches
 *
 * Validates that all item instances have correct equip_hash_chain values
 * matching their current equipment state. Detects corruption in equipment
 * state tracking or potential memory tampering.
 *
 * @param out Array to store detected mismatches (optional)
 * @param max_out Maximum number of mismatches to report
 * @return int Number of chain mismatches detected
 *
 * @note Compares stored vs expected hash values for all valid items
 * @note Set max_out to 0 to count mismatches without storing details
 * @note Only checks items that exist (skips empty instance slots)
 */
int rogue_integrity_scan_equip_chain_mismatches(RogueItemChainMismatch* out, int max_out)
{
    int written = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (!it)
            continue;
        unsigned long long expected = rogue_integrity_expected_item_equip_hash(i);
        if (expected != it->equip_hash_chain)
        {
            if (out && written < max_out)
            {
                out[written].inst_index = i;
                out[written].stored_chain = it->equip_hash_chain;
                out[written].expected_chain = expected;
            }
            written++;
        }
    }
    return written;
}

/**
 * @brief Scan for duplicate item GUIDs
 *
 * Validates that all item instances have unique GUIDs. Duplicate GUIDs
 * can indicate item duplication exploits, save file corruption, or
 * memory management issues.
 *
 * @param out Array to store indices of duplicate items (optional)
 * @param max_out Maximum number of duplicates to report
 * @return int Number of items with duplicate GUIDs detected
 *
 * @note Uses O(n²) algorithm due to small instance capacity
 * @note Set max_out to 0 to count duplicates without storing details
 * @note Only reports the first occurrence of each duplicate GUID
 * @note Invalid item instances are skipped during scanning
 */
int rogue_integrity_scan_duplicate_guids(int* out, int max_out)
{
    unsigned long long seen[ROGUE_ITEM_INSTANCE_CAP];
    int seen_count = 0;
    int dup_written = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (!it)
            continue;
        int duplicate = 0;
        for (int j = 0; j < seen_count; j++)
        {
            if (seen[j] == it->guid)
            {
                duplicate = 1;
                break;
            }
        }
        if (duplicate)
        {
            if (out && dup_written < max_out)
                out[dup_written] = i;
            dup_written++;
        }
        else
        {
            seen[seen_count++] = it->guid;
        }
    }
    return dup_written;
}
