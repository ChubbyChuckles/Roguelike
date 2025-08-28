#ifndef ROGUE_CORE_EFFECT_SPEC_LOAD_H
#define ROGUE_CORE_EFFECT_SPEC_LOAD_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Load EffectSpec definitions from external config.
     * Supported format:
     *  - JSON array (.json): [{
     *       "kind": "STAT_BUFF"|"DOT"|"AURA",
     *       "debuff": 0|1,
     *       "buff_type": "STAT_STRENGTH"|"POWER_STRIKE"|...,
     *       "magnitude": int,
     *       "duration_ms": number,
     *       "stack_rule": "ADD"|"REFRESH"|"EXTEND"|"UNIQUE"|"MULTIPLY"|"REPLACE_IF_STRONGER",
     *       "snapshot": 0|1,
     *       "scale_by_buff_type": "POWER_STRIKE"|"STAT_STRENGTH"|...,
     *       "scale_pct_per_point": int,
     *       "snapshot_scale": 0|1,
     *       "require_buff_type": "POWER_STRIKE"|...,
     *       "require_buff_min": int,
     *       "pulse_period_ms": number,
     *       "damage_type": "PHYSICAL"|"FIRE"|"FROST"|"ARCANE"|"POISON"|"BLEED"|"TRUE",
     *       "crit_mode": 0|1,
     *       "crit_chance_pct": 0..100,
     *       "aura_radius": number,
     *       "aura_group_mask": int
     *    }, ...]
     * Returns number of effects registered. Fills out_effect_ids if provided.
     */
    int rogue_effects_load_from_json_text(const char* json_text, int* out_effect_ids, int max_ids);

    /* Load from file path (expects .json). Returns count or <0 on error. */
    int rogue_effects_load_from_file(const char* path, int* out_effect_ids, int max_ids, char* err,
                                     int errcap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_EFFECT_SPEC_LOAD_H */
