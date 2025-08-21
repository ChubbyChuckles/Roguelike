/* Vendor System Phase 6: Special Offers & Rotations */
#ifndef ROGUE_VENDOR_SPECIAL_OFFERS_H
#define ROGUE_VENDOR_SPECIAL_OFFERS_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ROGUE_VENDOR_OFFER_SLOT_CAP
#define ROGUE_VENDOR_OFFER_SLOT_CAP 4
#endif

    typedef struct RogueVendorSpecialOffer
    {
        int def_index;              /* item definition offered */
        int rarity;                 /* rarity used for pricing */
        int base_price;             /* baseline price computed when rolled */
        unsigned int expires_at_ms; /* expiration time */
        int nemesis_bonus;          /* flagged if sourced via nemesis performance hook */
        int scarcity_boost;         /* flagged if scarcity influenced selection */
        int active;                 /* validity */
    } RogueVendorSpecialOffer;

    /* State per vendor archetype (simplified global single set) */
    void rogue_vendor_offers_reset(void);
    int rogue_vendor_offers_roll(unsigned int world_seed, unsigned int now_ms,
                                 int nemesis_defeated_flag);
    int rogue_vendor_offers_count(void);
    const RogueVendorSpecialOffer* rogue_vendor_offer_get(int index);
    /* Pity timer status (1 if guarantee will trigger next roll) */
    int rogue_vendor_offers_pity_pending(void);

#ifdef __cplusplus
}
#endif

#endif
