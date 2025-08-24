/* Vendor Reputation & Negotiation (Phase 4) */
#ifndef ROGUE_VENDOR_REPUTATION_H
#define ROGUE_VENDOR_REPUTATION_H

#include "vendor_registry.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueVendorRepState
    {
        int vendor_def_index;            /* -1 if unused */
        int reputation;                  /* accumulated rep points */
        unsigned int nego_attempts;      /* total negotiation attempts */
        unsigned int lockout_expires_ms; /* negotiation lockout expiry */
        int last_discount_pct;           /* last granted discount */
    } RogueVendorRepState;

    void rogue_vendor_rep_system_reset(void);
    int rogue_vendor_rep_gain(int vendor_def_index, int base_amount);
    void rogue_vendor_rep_adjust_raw(int vendor_def_index, int delta);
    int rogue_vendor_rep_current_tier(int vendor_def_index);
    float rogue_vendor_rep_progress(int vendor_def_index);
    int rogue_vendor_attempt_negotiation(int vendor_def_index, const char* negotiation_rule_id,
                                         int attr_str, int attr_dex, int attr_int, int attr_vit,
                                         unsigned int now_ms, unsigned int session_seed,
                                         int* out_success, int* out_locked);
    int rogue_vendor_rep_last_discount(int vendor_def_index);
    int rogue_vendor_rep_state_count(void);
    const RogueVendorRepState* rogue_vendor_rep_state_at(int idx);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VENDOR_REPUTATION_H */
