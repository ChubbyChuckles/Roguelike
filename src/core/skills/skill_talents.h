#ifndef ROGUE_CORE_SKILL_TALENTS_H
#define ROGUE_CORE_SKILL_TALENTS_H

#include "skills.h"
#include <stddef.h>

struct RogueProgressionMaze;

/* Minimal Phase 1.B talent graph skeleton (open allocation). */
typedef struct RogueTalentModifier
{
    int node_id;            /* talent node providing this modifier */
    int skill_id;           /* target skill id */
    float cd_scalar;        /* cooldown scalar multiplier (1.0 = no change) */
    int ap_delta;           /* additive AP cost delta */
    int add_tags;           /* bitmask to OR into tags */
    int charges_delta;      /* additive max_charges delta */
    int add_effect_spec_id; /* if >0, override effect_spec_id */
    float proc_chance_pct;  /* optional proc chance hint (reserved) */
} RogueTalentModifier;

/* Initialize with a progression maze reference (for gating + adjacency metadata). */
int rogue_talents_init(const struct RogueProgressionMaze* maze);
void rogue_talents_shutdown(void);

/* Configure open-allocation threshold: if >0, allow unlock when total unlocked >= threshold
 * even without direct predecessors (ANY threshold). */
void rogue_talents_set_any_threshold(int threshold);

/* Register a modifier linked to a node. Returns 1 on success. */
int rogue_talents_register_modifier(const RogueTalentModifier* mod);

/* Query if node is currently unlocked. */
int rogue_talents_is_unlocked(int node_id);

/* Check unlock preconditions (level/attr via maze + open allocation rule). */
int rogue_talents_can_unlock(int node_id, int level, int str, int dex, int intel, int vit);

/* Attempt to unlock a node, spending one talent point if available. Returns 1 on success. */
int rogue_talents_unlock(int node_id, unsigned int timestamp_ms, int level, int str, int dex,
                         int intel, int vit);

/* Serialize/deserialize unlocked bitset (versioned). Returns bytes written/read or -1. */
int rogue_talents_serialize(void* buffer, size_t buf_size);
int rogue_talents_deserialize(const void* buffer, size_t buf_size);

/* Deterministic hash over unlocked set (for diagnostics/replay). */
unsigned long long rogue_talents_hash(void);

/* Compute an "effective" skill definition with talent modifiers applied. Returns 1 on success. */
int rogue_skill_get_effective_def(int id, struct RogueSkillDef* out);

/* Respec API (Phase 1B.5 - partial):
    - rogue_talents_respec_last(n): undo the last n unlocks in reverse order, refunding points.
    - rogue_talents_full_respec(): undo all unlocks, refunding all spent points.
    Returns number of nodes actually respecced. */
int rogue_talents_respec_last(int n);
int rogue_talents_full_respec(void);

/* Preview (lightweight): Begin/cancel/commit a preview of unlocks without changing state until
    commit. Unlock rules are evaluated against current state + staged preview.
    Returns 1 on success, 0 on failure. */
int rogue_talents_preview_begin(void);
int rogue_talents_preview_unlock(int node_id, int level, int str, int dex, int intel, int vit);
int rogue_talents_preview_cancel(void);
int rogue_talents_preview_commit(unsigned int timestamp_ms);

#endif /* ROGUE_CORE_SKILL_TALENTS_H */
