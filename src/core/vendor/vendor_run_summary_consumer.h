/* Vendor & Crafting consumers for dungeon run summary callbacks (Phase 9 hooks) */
#ifndef ROGUE_VENDOR_RUN_SUMMARY_CONSUMER_H
#define ROGUE_VENDOR_RUN_SUMMARY_CONSUMER_H
#ifdef __cplusplus
extern "C"
{
#endif

    /* Initialize and register vendor/crafting listeners for dungeon run summary events. */
    void rogue_vendor_run_summary_listeners_init(void);
    /* Unregister/clear listeners (tests). */
    void rogue_vendor_run_summary_listeners_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
