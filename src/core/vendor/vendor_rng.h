/* Phase 12 Determinism & RNG Governance */
#ifndef ROGUE_VENDOR_RNG_H
#define ROGUE_VENDOR_RNG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Distinct stream identifiers to keep random domains isolated. */
    typedef enum RogueVendorRngStream
    {
        ROGUE_VENDOR_RNG_INVENTORY = 1,
        ROGUE_VENDOR_RNG_OFFERS = 2,
        ROGUE_VENDOR_RNG_NEGOTIATION = 3
    } RogueVendorRngStream;

    /* Compose a 32-bit seed from world_seed, vendor_id hash, refresh_epoch and stream id. */
    uint32_t rogue_vendor_seed_compose(uint32_t world_seed, const char* vendor_id,
                                       uint32_t refresh_epoch, RogueVendorRngStream stream);

    /* Minimal xorshift32 deterministic generator (inlined for speed). */
    static inline uint32_t rogue_vendor_xorshift32(uint32_t* s)
    {
        uint32_t x = *s;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        *s = x ? x : 0xA136AAADu;
        return *s;
    }

    /* Hash snapshot: stable hash of sorted inventory + price scalars context string. */
    uint32_t rogue_vendor_snapshot_hash(const int* def_indices, const int* rarities,
                                        const int* prices, int count, uint32_t world_seed,
                                        const char* vendor_id, uint32_t refresh_epoch,
                                        uint32_t price_mod_hash);

    /* Utility to compute FNV1a 32 of a byte buffer */
    uint32_t rogue_vendor_fnv1a32(const void* data, int len);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VENDOR_RNG_H */
