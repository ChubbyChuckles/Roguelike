#ifndef ROGUE_VENDOR_UI_FILTERS_H
#define ROGUE_VENDOR_UI_FILTERS_H
#ifdef __cplusplus
extern "C"
{
#endif

    /* Vendor UI filter persistence (Phase 16.5) */
    typedef struct RogueVendorUiFilters
    {
        int category_mask; /* bitmask over item categories (implementation-defined) */
        int min_rarity;    /* 0..4 */
        int max_rarity;    /* 0..4 */
        int stat_min;      /* generic stat lower bound (placeholder) */
        int stat_max;      /* generic stat upper bound (placeholder) */
    } RogueVendorUiFilters;

    /* Returns current filters snapshot (by value). */
    RogueVendorUiFilters rogue_vendor_ui_get_filters(void);
    /* Overwrite current filters (clamped) and persist to session store. */
    void rogue_vendor_ui_set_filters(const RogueVendorUiFilters* f);
    /* Reset filters to defaults. */
    void rogue_vendor_ui_reset_filters(void);

#ifdef __cplusplus
}
#endif
#endif
