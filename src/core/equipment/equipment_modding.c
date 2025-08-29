/**
 * @file equipment_modding.c
 * @brief Equipment modding system with sandboxed scripting and set comparison
 *
 * Phase 17.3: Sandboxed scripting hooks + Phase 17.4 set diff tool.
 * Provides a secure scripting system for custom stat modifications and
 * tools for comparing equipment set configurations.
 *
 * This module contains two main subsystems:
 * - Sandboxed Scripting (17.3): Safe execution of user-defined stat modifications
 * - Set Diff Tool (17.4): Comparison of equipment set configurations between versions
 *
 * @note All scripting operations are sandboxed with strict value limits
 * @note Set diff tool provides JSON output for external processing
 */

#include "equipment_modding.h"
#include "equipment_content.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Sandboxed Scripting System (Phase 17.3) ---- */

/**
 * @brief Convert stat name to internal index
 *
 * Maps human-readable stat names to internal stat indices used by the
 * sandboxed scripting system. Supports core stats and resistance types.
 *
 * @param name Stat name string (case-sensitive)
 * @return int Stat index (0-10) or -1 if not found
 *
 * @note Supported stats: strength, dexterity, vitality, intelligence,
 *       armor_flat, resist_fire, resist_cold, resist_light, resist_poison,
 *       resist_status, resist_physical
 */
static int stat_index(const char* name)
{
    static const char* names[] = {"strength",      "dexterity",      "vitality",
                                  "intelligence",  "armor_flat",     "resist_fire",
                                  "resist_cold",   "resist_light",   "resist_poison",
                                  "resist_status", "resist_physical"};
    for (int i = 0; i < (int) (sizeof(names) / sizeof(names[0])); ++i)
    {
        if (strcmp(name, names[i]) == 0)
            return i;
    }
    return -1;
}

/**
 * @brief Load sandboxed script from file
 *
 * Parses a script file containing stat modification instructions and
 * loads them into a sandboxed script structure. The script format supports
 * 'add' and 'mul' operations with strict value limits for security.
 *
 * Script format:
 * - Lines starting with # are comments
 * - Instructions: "add stat_name value" or "mul stat_name percentage"
 * - Maximum 16 instructions per script
 * - Add values: -1000 to 1000
 * - Mul percentages: -90 to 500
 *
 * @param path Path to the script file
 * @param out Output structure to store the loaded script
 * @return int Operation result:
 *         - 0: Success
 *         - -1: Invalid parameters, file error, or validation failure
 *
 * @note Script execution is sandboxed with strict limits to prevent exploits
 * @note Invalid instructions cause immediate load failure
 * @note File format is whitespace-tolerant but case-sensitive for stat names
 */
int rogue_script_load(const char* path, RogueSandboxScript* out)
{
    if (!path || !out)
        return -1;
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return -1;
    char line[128];
    out->instr_count = 0;
    while (fgets(line, sizeof line, f))
    {
        char* p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;
        char op[8];
        char stat[32];
        int val;
        int scanned;
#if defined(_MSC_VER)
        scanned = sscanf_s(p, "%7s %31s %d", op, (unsigned) _countof(op), stat,
                           (unsigned) _countof(stat), &val);
#else
        scanned = sscanf(p, "%7s %31s %d", op, stat, &val);
#endif
        if (scanned != 3)
        {
            fclose(f);
            return -1;
        }
        int sidx = stat_index(stat);
        if (sidx < 0)
        {
            fclose(f);
            return -1;
        }
        if (out->instr_count >= 16)
        {
            fclose(f);
            return -1;
        }
        /* Enforcement: additive values must be within -1000..1000; mul percent within -90..500 to
         * avoid runaway stats */
        if (strcmp(op, "add") == 0)
        {
            if (val < -1000 || val > 1000)
            {
                fclose(f);
                return -1;
            }
            RogueSandboxInstr* ins = &out->instrs[out->instr_count];
            ins->op = 1;
            ins->stat = (unsigned char) sidx;
            ins->value = val;
        }
        else if (strcmp(op, "mul") == 0)
        {
            if (val < -90 || val > 500)
            {
                fclose(f);
                return -1;
            }
            RogueSandboxInstr* ins = &out->instrs[out->instr_count];
            ins->op = 2;
            ins->stat = (unsigned char) sidx;
            ins->value = val;
        }
        else
        {
            fclose(f);
            return -1;
        }
        out->instr_count++;
    }
    fclose(f);
    return 0;
}

/**
 * @brief Calculate hash of sandboxed script
 *
 * Generates a deterministic hash of the script's instruction sequence
 * for change detection, caching, or comparison purposes.
 *
 * @param s Pointer to the script to hash
 * @return unsigned int 32-bit hash value (0 for invalid script)
 *
 * @note Uses FNV-1a hash algorithm for good distribution
 * @note Hash includes all instruction data (op, stat, value)
 * @note Used for script versioning and integrity checking
 */
unsigned int rogue_script_hash(const RogueSandboxScript* s)
{
    if (!s)
        return 0u;
    unsigned long long h = 1469598103934665603ull;
    for (int i = 0; i < s->instr_count; i++)
    {
        const RogueSandboxInstr* ins = &s->instrs[i];
        const unsigned char* b = (const unsigned char*) ins;
        for (size_t k = 0; k < sizeof(*ins); k++)
        {
            h ^= b[k];
            h *= 1099511628211ull;
        }
    }
    return (unsigned int) (h ^ (h >> 32));
}

/**
 * @brief Apply sandboxed script to stat values
 *
 * Executes a loaded script against the provided stat values, applying
 * all add and multiply operations in the correct order. Uses a two-pass
 * system: first pass applies additions, second pass applies multiplications.
 *
 * @param s Script to execute
 * @param strength Pointer to strength stat
 * @param dexterity Pointer to dexterity stat
 * @param vitality Pointer to vitality stat
 * @param intelligence Pointer to intelligence stat
 * @param armor_flat Pointer to flat armor stat
 * @param r_fire Pointer to fire resistance
 * @param r_cold Pointer to cold resistance
 * @param r_light Pointer to light resistance
 * @param r_poison Pointer to poison resistance
 * @param r_status Pointer to status resistance
 * @param r_phys Pointer to physical resistance
 *
 * @note Two-pass execution ensures correct order of operations
 * @note NULL stat pointers are safely ignored
 * @note Multiplication is percentage-based (value/100)
 * @note Invalid stat indices are ignored during execution
 */
void rogue_script_apply(const RogueSandboxScript* s, int* strength, int* dexterity, int* vitality,
                        int* intelligence, int* armor_flat, int* r_fire, int* r_cold, int* r_light,
                        int* r_poison, int* r_status, int* r_phys)
{
    if (!s)
        return;
    int* map[] = {
        strength, dexterity, vitality, intelligence, armor_flat, r_fire,
        r_cold,   r_light,   r_poison, r_status,     r_phys}; /* first pass adds, second mul */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < s->instr_count; i++)
        {
            const RogueSandboxInstr* ins = &s->instrs[i];
            int* dst = map[ins->stat];
            if (!dst)
                continue;
            if (pass == 0 && ins->op == 1)
            {
                *dst += ins->value;
            }
            else if (pass == 1 && ins->op == 2)
            {
                *dst += (*dst * ins->value) / 100;
            }
        }
    }
}

/* ---- Set Diff Tool (Phase 17.4) ---- */

/**
 * @brief Temporary structure for set comparison
 */
typedef struct TmpSet
{
    int id;         /**< Set ID */
    int bonus_hash; /**< Hash of set bonuses for change detection */
} TmpSet;

/**
 * @brief Load sets from file into temporary array
 *
 * Loads equipment sets from a JSON file and creates a temporary array
 * of set information for comparison purposes. Calculates bonus hashes
 * for change detection.
 *
 * @param path Path to the JSON file containing set definitions
 * @param arr Output array for temporary set data
 * @param cap Maximum capacity of the output array
 * @return int Number of sets loaded, or -1 on error
 *
 * @note Temporarily modifies the global set registry
 * @note Calculates FNV-1a hash of bonus data for comparison
 * @note Limited to first 'cap' sets if file contains more
 */
static int load_sets_temp(const char* path, TmpSet* arr, int cap)
{
    int before = rogue_set_count();
    int added = rogue_sets_load_from_json(path);
    if (added < 0)
        return -1;
    int after = rogue_set_count();
    int newc = after - before;
    if (newc > cap)
        newc = cap;
    int outc = 0;
    for (int i = before; i < after && outc < cap; i++)
    {
        const RogueSetDef* d = rogue_set_at(i);
        unsigned long long h = 1469598103934665603ull;
        const unsigned char* bytes = (const unsigned char*) d->bonuses;
        size_t len = sizeof(d->bonuses);
        for (size_t k = 0; k < len; k++)
        {
            h ^= bytes[k];
            h *= 1099511628211ull;
        }
        arr[outc].id = d->set_id;
        arr[outc].bonus_hash = (int) (h ^ (h >> 32));
        outc++;
    }
    return outc;
}

/**
 * @brief Compare equipment sets between two configurations
 *
 * Performs a diff operation between two equipment set files, identifying
 * added, removed, and modified sets. Outputs results in JSON format for
 * external processing or display.
 *
 * The comparison identifies:
 * - Added sets: Present in mod_path but not in base_path
 * - Removed sets: Present in base_path but not in mod_path
 * - Changed sets: Present in both but with different bonus configurations
 *
 * @param base_path Path to base set configuration file
 * @param mod_path Path to modified set configuration file
 * @param out Output buffer for JSON result
 * @param cap Capacity of output buffer
 * @return int Length of JSON output on success, -1 on error
 *
 * @note Output format: {"added":[ids],"removed":[ids],"changed":[ids]}
 * @note Modifies global set registry temporarily (resets after comparison)
 * @note Requires at least 32 bytes of output buffer capacity
 * @note Uses bonus hash comparison to detect set modifications
 *
 * @warning Global set registry is reset after comparison for cleanliness
 */
int rogue_sets_diff(const char* base_path, const char* mod_path, char* out, int cap)
{
    if (!base_path || !mod_path || !out || cap < 32)
        return -1;
    int orig = rogue_set_count();
    (void) orig;
    TmpSet base[64];
    TmpSet mod[64];
    int base_added = load_sets_temp(base_path, base, 64);
    if (base_added < 0)
        return -1;
    int mod_added = load_sets_temp(mod_path, mod, 64);
    if (mod_added < 0)
        return -1; /* registry now includes both sets; reset for cleanliness */
    rogue_sets_reset();
    int added_ids[64];
    int removed_ids[64];
    int changed_ids[64];
    int added_n = 0, removed_n = 0, changed_n = 0;
    for (int i = 0; i < mod_added; i++)
    {
        int id = mod[i].id;
        int found = -1;
        for (int j = 0; j < base_added; j++)
        {
            if (base[j].id == id)
            {
                found = j;
                break;
            }
        }
        if (found < 0)
            added_ids[added_n++] = id;
        else if (base[found].bonus_hash != mod[i].bonus_hash)
            changed_ids[changed_n++] = id;
    }
    for (int i = 0; i < base_added; i++)
    {
        int id = base[i].id;
        int found = -1;
        for (int j = 0; j < mod_added; j++)
        {
            if (mod[j].id == id)
            {
                found = j;
                break;
            }
        }
        if (found < 0)
            removed_ids[removed_n++] = id;
    }
    int off = 0;
    int n = snprintf(out + off, (size_t) cap - off, "{\"added\":[");
    if (n < 0)
        return -1;
    off += n;
    for (int i = 0; i < added_n; i++)
    {
        n = snprintf(out + off, (size_t) cap - off, "%s%d", i ? "," : "", added_ids[i]);
        if (n < 0 || off + n >= cap)
            return -1;
        off += n;
    }
    n = snprintf(out + off, (size_t) cap - off, "],\"removed\":[");
    if (n < 0 || off + n >= cap)
        return -1;
    off += n;
    for (int i = 0; i < removed_n; i++)
    {
        n = snprintf(out + off, (size_t) cap - off, "%s%d", i ? "," : "", removed_ids[i]);
        if (n < 0 || off + n >= cap)
            return -1;
        off += n;
    }
    n = snprintf(out + off, (size_t) cap - off, "],\"changed\":[");
    if (n < 0 || off + n >= cap)
        return -1;
    off += n;
    for (int i = 0; i < changed_n; i++)
    {
        n = snprintf(out + off, (size_t) cap - off, "%s%d", i ? "," : "", changed_ids[i]);
        if (n < 0 || off + n >= cap)
            return -1;
        off += n;
    }
    if (off + 3 >= cap)
        return -1;
    out[off++] = ']';
    out[off++] = '}';
    out[off] = '\0';
    return off;
}
