#ifndef ROGUE_DEBUG_OVERLAY_PREFS_H
#define ROGUE_DEBUG_OVERLAY_PREFS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Minimal persisted key->int preferences for overlay (e.g., splitter widths). */
    void overlay_prefs_init(void);
    int overlay_prefs_get_int(const char* key, int default_value);
    void overlay_prefs_set_int(const char* key, int value);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_DEBUG_OVERLAY_PREFS_H */
