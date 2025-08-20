/* Phase 9 Synergy Layer (Equipment & Skills Integration)
 * Provides deterministic ordering utilities for combining layered modifiers
 * and simple tag‑based synergy helpers bridging equipment state -> passive effects.
 */
#ifndef ROGUE_PROGRESSION_SYNERGY_H
#define ROGUE_PROGRESSION_SYNERGY_H

#include "entities/player.h"
#include "core/skills.h" /* for tag bits */

/* Layered damage aggregation (Phase 9.1) applying documented order:
   Base -> Equipment -> Passives -> Mastery -> Perpetual (micro) */
float rogue_progression_layered_damage(float base_flat, float equipment_pct, float passive_pct, float mastery_pct, float micro_pct);

/* Simple linear attribute aggregation (kept for symmetry & future auditing). */
int rogue_progression_layered_strength(int base_val, int equipment_bonus, int passive_bonus, int mastery_bonus, int micro_bonus);

/* Cap enforcement (Phase 9.2) */
int rogue_progression_final_crit_chance(int flat_crit_percent);
float rogue_progression_final_cdr(float raw_cdr_percent);

/* Tag mask derived from equipment / player state (Phase 9.3+9.4).
   Currently maps weapon infusion elemental types to skill tags. */
int rogue_progression_synergy_tag_mask(const RoguePlayer* p);

/* Conditional fire damage bonus: returns passive_fire_bonus if FIRE tag present else 0. */
int rogue_progression_synergy_fire_bonus(int tag_mask, int passive_fire_bonus);

#endif
