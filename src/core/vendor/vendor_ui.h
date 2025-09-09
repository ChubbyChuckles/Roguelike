#ifndef ROGUE_VENDOR_UI_H
#define ROGUE_VENDOR_UI_H
#ifdef __cplusplus
extern "C"
{
#endif
    /* Returns current vendor restock progress fraction in [0,1]. */
    float rogue_vendor_restock_fraction(void);
    /* Render vendor panel (no return). */
    void rogue_vendor_panel_render(void);
    /* Render equipment panel. */
    void rogue_equipment_panel_render(void);
    /* Phase 16.1: Tab controls */
    enum RogueVendorTab
    {
        ROGUE_VENDOR_TAB_BUY = 0,
        ROGUE_VENDOR_TAB_SELL = 1,
        ROGUE_VENDOR_TAB_BUYBACK = 2,
        ROGUE_VENDOR_TAB_SPECIAL = 3
    };
    int rogue_vendor_tab_get(void);
    void rogue_vendor_tab_set(int tab);
    /* Phase 16.3/16.4: lightweight helpers for UI wiring (exposed for tests) */
    int rogue_vendor_rep_current_tier(int vendor_def_index);
    float rogue_vendor_rep_progress(int vendor_def_index);
    int rogue_vendor_rep_last_discount(int vendor_def_index);
    /* Negotiation preview UI helper */
    int rogue_vendor_ui_negotiation_preview(const char* rule_id, int attr_str, int attr_dex,
                                            int attr_int, int attr_vit, int* out_min_pct,
                                            int* out_max_pct, float* out_success_prob);
#ifdef __cplusplus
}
#endif
#endif
