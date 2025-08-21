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
#ifdef __cplusplus
}
#endif
#endif
