/* Character Progression Stat Taxonomy (Phase 0)
 * Provides a canonical, stable, data‑driven enumeration of all player progression / combat stats.
 * Goals (Phase 0):
 *  - Enumerate existing stats in codebase (primary attributes, derived outputs, resistances)
 *  - Define taxonomy categories & reserved ID ranges to avoid future collisions
 *  - Expose read‑only registry APIs with stable ordering & deterministic fingerprint
 *  - Supply unit tests verifying uniqueness, ordering stability, reserved gaps, & serialization
 * order
 *
 * ID Ranges (all inclusive):
 *   0   –  99 : Primary Attributes (base allocatable)
 *   100 – 199 : Derived / Computed (output of formulas / aggregations)
 *   200 – 299 : Rating (subject to diminishing returns curves; future Phase 3)
 *   300 – 399 : Modifiers (generic percentage or scalar modifiers; future phases)
 *   400 – 499 : Reserved (future expansion: mastery, micro‑nodes, etc.)
 */
#ifndef ROGUE_PROGRESSION_STATS_H
#define ROGUE_PROGRESSION_STATS_H

#include <stddef.h>

enum RogueStatCategory
{
    ROGUE_STAT_PRIMARY = 0,
    ROGUE_STAT_DERIVED = 1,
    ROGUE_STAT_RATING = 2,
    ROGUE_STAT_MODIFIER = 3,
    ROGUE_STAT__COUNT
};

/* Definition entry for a stat. */
typedef struct RogueStatDef
{
    int id;                          /* Stable numeric ID (see ranges above) */
    const char* code;                /* Short machine code (uppercase, snake-ish) */
    const char* name;                /* Human friendly name */
    enum RogueStatCategory category; /* Taxonomy category */
    int reserved;                    /* 1 if placeholder / not yet live in formulas */
} RogueStatDef;

/* Return pointer to contiguous, immutable array of stat definitions. *count receives element count.
 */
const RogueStatDef* rogue_stat_def_all(size_t* count);

/* Lookup by numeric ID; returns NULL if not found. */
const RogueStatDef* rogue_stat_def_by_id(int id);

/* Deterministic hash/fingerprint of the full ordered registry (codes + ids + categories + reserved
 * flags). */
unsigned long long rogue_stat_registry_fingerprint(void);

#endif /* ROGUE_PROGRESSION_STATS_H */
