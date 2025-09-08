/* Phase 13.2: Versioned economy header (curve/margin policy versions) */
#ifndef ROGUE_ECONOMY_VERSION_H
#define ROGUE_ECONOMY_VERSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueEconomyHeader
    {
        uint32_t curve_version;         /* version tag for value/curve tuning */
        uint32_t margin_policy_version; /* version tag for pricing margin policy */
        uint32_t reserved0;             /* reserved for future expansion */
    } RogueEconomyHeader;

    /* Reset to defaults (zeros). */
    void rogue_economy_version_reset(void);

    /* Set fields explicitly. */
    void rogue_economy_version_set(uint32_t curve_ver, uint32_t margin_ver);

    /* Get current header snapshot. */
    RogueEconomyHeader rogue_economy_version_get(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ECONOMY_VERSION_H */
