#ifndef ROGUE_LOOT_AFFIXES_H
#define ROGUE_LOOT_AFFIXES_H

#include <stdint.h>

#define ROGUE_MAX_AFFIXES 256

typedef enum RogueAffixType { ROGUE_AFFIX_PREFIX=0, ROGUE_AFFIX_SUFFIX=1 } RogueAffixType;
typedef enum RogueAffixStat {
    ROGUE_AFFIX_STAT_NONE=0,
    ROGUE_AFFIX_STAT_DAMAGE_FLAT,
    ROGUE_AFFIX_STAT_AGILITY_FLAT, /* legacy alias for dexterity until full rename */
    ROGUE_AFFIX_STAT_STRENGTH_FLAT,
    ROGUE_AFFIX_STAT_DEXTERITY_FLAT,
    ROGUE_AFFIX_STAT_VITALITY_FLAT,
    ROGUE_AFFIX_STAT_INTELLIGENCE_FLAT,
    ROGUE_AFFIX_STAT_ARMOR_FLAT,
    ROGUE_AFFIX_STAT_RESIST_PHYSICAL,
    ROGUE_AFFIX_STAT_RESIST_FIRE,
    ROGUE_AFFIX_STAT_RESIST_COLD,
    ROGUE_AFFIX_STAT_RESIST_LIGHTNING,
    ROGUE_AFFIX_STAT_RESIST_POISON,
    ROGUE_AFFIX_STAT_RESIST_STATUS,
    /* Phase 7 defensive extensions */
    ROGUE_AFFIX_STAT_BLOCK_CHANCE,
    ROGUE_AFFIX_STAT_BLOCK_VALUE,
    ROGUE_AFFIX_STAT__COUNT
} RogueAffixStat;

typedef struct RogueAffixDef {
    char id[48];
    RogueAffixType type;
    RogueAffixStat stat;
    int min_value;
    int max_value;
    int weight_per_rarity[5]; /* per rarity tier weight */
} RogueAffixDef;

int rogue_affixes_reset(void);
int rogue_affixes_load_from_cfg(const char* path); /* returns number added */
int rogue_affix_count(void);
const RogueAffixDef* rogue_affix_at(int index);
int rogue_affix_index(const char* id);
int rogue_affix_roll(RogueAffixType type, int rarity, unsigned int* rng_state);
/* Roll a concrete stat value for an already selected affix index using its min/max range.
    Returns rolled value (inclusive range) or -1 on error. Deterministic given rng_state. */
int rogue_affix_roll_value(int affix_index, unsigned int* rng_state);
/* (8.4) Variant that biases result upward by a quality scalar (>1 pushes toward ceiling).
    quality_scalar <=1 behaves like baseline uniform. */
int rogue_affix_roll_value_scaled(int affix_index, unsigned int* rng_state, float quality_scalar);

/* Tooling Phase 21.3: Export affix range summary as JSON array of objects.
    Format: [{"id":"...","type":0|1,"stat":N,"min":X,"max":Y,"w":[w0,w1,w2,w3,w4]}]
    Returns number of bytes written (excluding NUL) or -1 on error; always NUL-terminates if cap>0. */
int rogue_affixes_export_json(char* buf, int cap);

/* Tooling Phase 21.3: Export affix range summary as JSON array of objects.
    Format: [{"id":"...","type":0|1,"stat":N,"min":X,"max":Y,"w":[w0,w1,w2,w3,w4]},...]
    Returns number of bytes written (excluding NUL) or -1 on error; always NUL-terminates if cap>0. */
int rogue_affixes_export_json(char* buf, int cap);

#endif
