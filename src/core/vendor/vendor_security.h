/* Phase 15: Security / Anti-Exploit guards */
#ifndef ROGUE_VENDOR_SECURITY_H
#define ROGUE_VENDOR_SECURITY_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    /* Reset all security state (tests). */
    void rogue_vendor_security_reset(void);

    /* Record a completed purchase from vendor -> player at time now_ms. */
    void rogue_vendor_security_note_purchase(uint32_t now_ms);

    /* Returns a temporary scalar >=1.0 that increases prices under purchase sprees. Capped at +5%.
     */
    float rogue_vendor_security_spree_scalar(void);

    /* Combined exploit guard scalar (currently equals spree scalar; subject to extension). */
    float rogue_vendor_security_exploit_scalar(void);

    /* Negotiation spam guard: returns 1 if allowed, 0 if blocked and applies an extended lockout.
     */
    int rogue_vendor_security_negotiation_allowed(int vendor_def_index, uint32_t now_ms);

    /* Remaining extended lockout time (ms) if any for negotiations; 0 when clear. */
    uint32_t rogue_vendor_security_negotiation_lockout_remaining(int vendor_def_index,
                                                                 uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VENDOR_SECURITY_H */
