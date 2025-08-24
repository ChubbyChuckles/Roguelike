/* loot_api_doc.c - Itemsystem Phase 23.5 (API doc generation)
 * A compact curated list of public loot APIs with one-line descriptions so
 * tooling / editors can surface quick reference or export to file for docs.
 */
#include "loot_api_doc.h"
#include <stdio.h>
#include <string.h>

typedef struct ApiEntry
{
    const char* name;
    const char* desc;
} ApiEntry;

/* NOTE: Keep ordering stable (alphabetical by primary module then function). */
static const ApiEntry k_entries[] = {
    {"rogue_affix_roll", "Select a random affix index of requested type at rarity (weights)."},
    {"rogue_affix_roll_value", "Roll concrete value within affix min/max uniformly."},
    {"rogue_item_defs_load_from_cfg", "Load item definitions from CSV-like config file."},
    {"rogue_item_defs_load_directory", "Load multiple category item def files from a directory."},
    {"rogue_loot_roll_hash", "Compute verification hash for a loot roll (security)."},
    {"rogue_loot_security_snapshot_files", "Snapshot combined hash of core loot config files."},
    {"rogue_loot_security_verify_files",
     "Verify config files vs last snapshot (tamper detection)."},
    {"rogue_loot_anomaly_flag", "Query rarity spike anomaly detector flag."},
    {"rogue_loot_anomaly_record", "Record a set of drop rarities into spike detector."},
    {"rogue_loot_anomaly_config", "Configure anomaly detector window + thresholds."},
    {"rogue_loot_security_set_server_mode",
     "Enable/disable server authoritative verification mode."},
    {"rogue_loot_server_verify", "Validate client-reported roll against recomputed hash."},
    {"rogue_loot_filter_refresh_instances",
     "Reapply loot filter predicates to existing ground items."},
    {"rogue_loot_tables_load_from_cfg", "Load loot table definitions from config file."},
    {"rogue_loot_tables_roll",
     "Perform weighted loot table roll producing item indices + quantities."},
};

int rogue_loot_generate_api_doc(char* buf, int cap)
{
    if (!buf || cap <= 0)
        return -1;
    if (cap < 128)
    {
        buf[0] = '\0';
        return -1;
    }
    int written = 0;
    written += snprintf(buf + written, cap - written, "LOOT API REFERENCE (curated)\n");
    for (size_t i = 0; i < sizeof(k_entries) / sizeof(k_entries[0]); ++i)
    {
        const ApiEntry* e = &k_entries[i];
        int r = snprintf(buf + written, cap - written, "%s: %s\n", e->name, e->desc);
        if (r < 0)
            break;
        if (r >= cap - written)
        { /* truncate */
            written = cap - 1;
            break;
        }
        written += r;
    }
    if (written >= cap)
        written = cap - 1;
    buf[written] = '\0';
    return written;
}
