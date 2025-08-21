/* Vendor System Phase 5: Buyback Ring Buffer & Depreciation */
#ifndef ROGUE_VENDOR_BUYBACK_H
#define ROGUE_VENDOR_BUYBACK_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Capacity of per-vendor buyback buffer (recent items sold by player). */
#ifndef ROGUE_VENDOR_BUYBACK_CAP
#define ROGUE_VENDOR_BUYBACK_CAP 32
#endif

    typedef struct RogueVendorBuybackEntry
    {
        unsigned long long item_guid; /* instance guid */
        int item_def_index;
        int rarity;
        int category;
        int condition_pct;               /* condition when sold */
        int original_price;              /* price paid to player */
        unsigned int sell_time_ms;       /* game time when sold */
        unsigned int assimilate_time_ms; /* scheduled assimilation time */
        int active;                      /* 1 if slot occupied */
    } RogueVendorBuybackEntry;

    /* Reset global buyback state (all vendors). */
    void rogue_vendor_buyback_reset(void);

    /* Record a sale from player -> vendor for potential buyback. Returns slot index or -1. */
    int rogue_vendor_buyback_record(int vendor_def_index, unsigned long long item_guid,
                                    int item_def_index, int rarity, int category, int condition_pct,
                                    int price, unsigned int now_ms);

    /* Enumerate active buyback entries for a vendor; returns count written. */
    int rogue_vendor_buyback_list(int vendor_def_index, RogueVendorBuybackEntry* out, int max_out,
                                  unsigned int now_ms);

    /* Attempt to buy back an item (player purchasing it). Returns price to pay or -1 if
     * unavailable. */
    int rogue_vendor_buyback_purchase(int vendor_def_index, unsigned long long item_guid,
                                      unsigned int now_ms);

    /* Depreciated price query for UI (current price vendor will sell back to player). */
    int rogue_vendor_buyback_current_price(const RogueVendorBuybackEntry* e, unsigned int now_ms);

    /* Assimilate any expired entries (moves them into normal vendor inventory implicitly). */
    void rogue_vendor_buyback_tick(int vendor_def_index, unsigned int now_ms);

    /* Anti-duplicate GUID lineage check: returns 1 if guid recently seen in buyback set (possible
     * exploit), 0 otherwise. */
    int rogue_vendor_buyback_guid_recent(unsigned long long guid);

    /* Transaction journal integration (Phase 5.3) */
    void rogue_vendor_tx_journal_record(int vendor_def_index, unsigned long long item_guid,
                                        int action_code, int price, int rep_delta,
                                        int discount_pct);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VENDOR_BUYBACK_H */
