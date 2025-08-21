#ifndef ROGUE_LOOT_TABLES_H
#define ROGUE_LOOT_TABLES_H
#include <stdint.h>

#define ROGUE_MAX_LOOT_TABLE_ID 32
#define ROGUE_MAX_LOOT_ENTRIES 32
#define ROGUE_MAX_LOOT_TABLES 128

/* Phase 1 simplified: only direct item drops with weight + qty range */

typedef struct RogueLootEntry {
    int item_def_index; /* index into item defs registry */
    int weight;         /* relative weight */
    int qmin, qmax;     /* quantity range */
    int rarity_min;     /* -1 = use item def */
    int rarity_max;     /* inclusive; if < rarity_min use rarity_min */
} RogueLootEntry;

typedef struct RogueLootTableDef {
    char id[ROGUE_MAX_LOOT_TABLE_ID];
    int entry_count;
    RogueLootEntry entries[ROGUE_MAX_LOOT_ENTRIES];
    int rolls_min; /* number of selection rolls (e.g. 1) */
    int rolls_max; /* can be > rolls_min for variability */
} RogueLootTableDef;

int rogue_loot_tables_reset(void);
int rogue_loot_tables_load_from_cfg(const char* path); /* returns number added */
const RogueLootTableDef* rogue_loot_table_by_id(const char* id);
int rogue_loot_table_index(const char* id);
int rogue_loot_tables_count(void);

/* Roll API (Phase 1): returns number of concrete drops produced into out arrays */
int rogue_loot_roll(int table_index, unsigned int* rng_state, int max_out,
                    int* out_item_def_indices, int* out_quantities);

/* Extended variant supplying rarities (optional). If out_rarities provided entries will be filled with rolled rarity or -1 for default. */
int rogue_loot_roll_ex(int table_index, unsigned int* rng_state, int max_out,
                    int* out_item_def_indices, int* out_quantities, int* out_rarities);

/* Simple RNG (pcg-ish LCG fallback) */
static inline unsigned int rogue_rng_next(unsigned int* s){ *s = (*s * 1664525u) + 1013904223u; return *s; }
static inline int rogue_rng_range(unsigned int* s, int hi_exclusive){ return (int)(rogue_rng_next(s) % (unsigned)hi_exclusive); }

/* Exposed rarity sampler (Phase 5.3) for independent statistical testing */
int rogue_loot_rarity_sample(unsigned int* rng_state, int rmin, int rmax);

#endif
