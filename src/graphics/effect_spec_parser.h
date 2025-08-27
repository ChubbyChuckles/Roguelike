/* effect_spec_parser.h - Phase 3.1 EffectSpec parser (in-memory text and file) */
#ifndef ROGUE_CORE_EFFECT_SPEC_PARSER_H
#define ROGUE_CORE_EFFECT_SPEC_PARSER_H
#ifdef __cplusplus
extern "C"
{
#endif

    /* Parse a key/value text and register EffectSpecs.
     * Format: lines like
     *   effect.<index>.kind = STAT_BUFF
     *   effect.<index>.buff_type = STAT_STRENGTH | POWER_STRIKE
     *   effect.<index>.magnitude = 5
     *   effect.<index>.duration_ms = 1000
     *   effect.<index>.stack_rule = ADD|REFRESH|EXTEND|UNIQUE|MULTIPLY|REPLACE_IF_STRONGER
     *   effect.<index>.snapshot = 0|1
     *   effect.<index>.scale_by_buff_type = POWER_STRIKE|STAT_STRENGTH
     *   effect.<index>.scale_pct_per_point = <int>   # percent per buff point (e.g., 10)
     *   effect.<index>.snapshot_scale = 0|1          # 1 = snapshot scale for pulses
     *   effect.<index>.require_buff_type = POWER_STRIKE|STAT_STRENGTH
     *   effect.<index>.require_buff_min = <int>
     *   effect.<index>.pulse_period_ms = 0|100|...
     * Unknown keys are ignored for forward compatibility.
     * Returns number of effects registered, fills out_effect_ids (optional) in ascending index
     * order. On error, returns -1 and writes a brief message into err (if provided).
     */
    int rogue_effects_parse_text(const char* text, int* out_effect_ids, int max_ids, char* err,
                                 int errcap);

    /* Parse from file path. Same semantics as above. */
    int rogue_effects_parse_file(const char* path, int* out_effect_ids, int max_ids, char* err,
                                 int errcap);

#ifdef __cplusplus
}
#endif
#endif
