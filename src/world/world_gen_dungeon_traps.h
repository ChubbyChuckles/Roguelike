#ifndef ROGUE_WORLD_GEN_DUNGEON_TRAPS_H
#define ROGUE_WORLD_GEN_DUNGEON_TRAPS_H

#include "world_gen.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Phase 7: Hazard & Trap System (initial slice)
     * Minimal, deterministic trap definitions + helpers for damage scaling,
     * overlap resolution, and disarm interaction.
     */

    typedef enum RogueTrapTriggerType
    {
        ROGUE_TRAP_TRIGGER_PRESSURE_PLATE = 0,
        ROGUE_TRAP_TRIGGER_PROXIMITY = 1,
        ROGUE_TRAP_TRIGGER_TIMED = 2
    } RogueTrapTriggerType;

    typedef struct RogueTrapDef
    {
        char id[24];
        RogueTrapTriggerType trigger;
        int telegraph_ms; /* pre-trigger windup */
        int damage_base;  /* base damage at ΔL=0, before avoidance */
        int cooldown_ms;  /* retrigger cooldown */
        int disarm_diff;  /* 0..100 difficulty */
    } RogueTrapDef;

    /* Parse a minimal trap definition JSON (keys: id, trigger, telegraph_ms, damage_base,
     * cooldown_ms, disarm_diff). Returns 1 on success; err populated on failure. */
    int rogue_trap_def_load_json_text(const char* json_text, RogueTrapDef* out, char* err,
                                      size_t err_cap);

    /* Compute scaled damage for a trap given target relative level delta (ΔL) and
     * player avoidance stat (0..100). Monotone: increases with ΔL, decreases with avoidance. */
    int rogue_trap_compute_damage(const RogueTrapDef* def, int delta_level, int player_avoid);

    /* Resolve overlapping high-density traps by capping traps per 3x3 window to max_density.
     * Excess traps are converted to dungeon floor. Returns number of traps removed. */
    int rogue_trap_resolve_overlap(RogueTileMap* io_map, int max_density);

    /* Probabilistic disarm check. Returns 1 if disarm succeeds, 0 otherwise. Deterministic for ctx.
     */
    int rogue_trap_disarm_success(RogueWorldGenContext* ctx, const RogueTrapDef* def,
                                  int player_disarm_skill);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_DUNGEON_TRAPS_H */
