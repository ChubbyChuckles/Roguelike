/* Phase 16.3: Negotiation UI preview helpers (headless/testable)
   Computes a simple success probability estimate and exposes rule bounds for UI display. */
#ifndef ROGUE_VENDOR_UI_NEGOTIATION_H
#define ROGUE_VENDOR_UI_NEGOTIATION_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Returns 0 on failure (no rules), 1 on success.
       If rule_id is NULL, uses the first negotiation rule in the registry.
       Outputs:
         - out_min_pct/out_max_pct: the rule's discount bounds (inclusive)
         - out_success_prob: approximate chance [0,1] given attributes and rule min_roll
       Success probability model matches the deterministic check: roll=d20+avg(skill_tags),
       model P = clamp((21 - (min_roll - avg_score)) / 20, 0..1). */
    int rogue_vendor_ui_negotiation_preview(const char* rule_id, int attr_str, int attr_dex,
                                            int attr_int, int attr_vit, int* out_min_pct,
                                            int* out_max_pct, float* out_success_prob);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VENDOR_UI_NEGOTIATION_H */
