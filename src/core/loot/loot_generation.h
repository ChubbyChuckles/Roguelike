/* Advanced item generation context & API (8.1) */
#ifndef ROGUE_LOOT_GENERATION_H
#define ROGUE_LOOT_GENERATION_H
#include <stdint.h>

typedef struct RogueGenerationContext {
    int enemy_level; /* influencing rarity floor / quality */
    int biome_id;    /* future use: biome-specific affix gating */
    int enemy_archetype; /* influences seed mixing */
    int player_luck; /* scalar 0..N affects quality scalar */
} RogueGenerationContext;

/* Result of a generation request */
typedef struct RogueGeneratedItem {
    int def_index;
    int rarity;
    int inst_index; /* spawned ground instance index or -1 if not spawned */
} RogueGeneratedItem;

/* Set global generation tuning parameters */
void rogue_generation_set_quality_scalar(float qs_min, float qs_max);

/* Core API: generate one item from a loot table with context; returns 0 on success and fills out. */
int rogue_generate_item(int loot_table_index, const RogueGenerationContext* ctx, unsigned int* rng_state, RogueGeneratedItem* out);

/* Seed mixing helper (8.5) */
unsigned int rogue_generation_mix_seed(const RogueGenerationContext* ctx, unsigned int base_seed);

#endif
