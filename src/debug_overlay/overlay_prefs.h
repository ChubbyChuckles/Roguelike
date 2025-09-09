/**
 * @file overlay_prefs.h
 * @brief Persistent preferences system for the debug overlay.
 *
 * Provides a minimal key-value preference storage system for the debug overlay,
 * allowing UI components to persist simple integer settings like splitter
 * widths, panel states, and other configuration values across sessions.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#ifndef ROGUE_DEBUG_OVERLAY_PREFS_H
#define ROGUE_DEBUG_OVERLAY_PREFS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the overlay preferences system.
     *
     * Sets up the preferences storage system for the debug overlay.
     * Must be called before using any other preference functions.
     * This initializes internal data structures and may load existing
     * preferences from persistent storage.
     */
    void overlay_prefs_init(void);
    
    /**
     * @brief Retrieve an integer preference value.
     *
     * Gets the value associated with the specified preference key.
     * If the key doesn't exist, returns the provided default value.
     *
     * @param key The preference key to look up (null-terminated string)
     * @param default_value The value to return if the key is not found
     * @return The stored preference value, or default_value if key not found
     */
    int overlay_prefs_get_int(const char* key, int default_value);
    
    /**
     * @brief Set an integer preference value.
     *
     * Stores an integer value associated with the specified preference key.
     * If the key already exists, its value is updated. The preference
     * may be persisted to storage depending on the implementation.
     *
     * @param key The preference key to set (null-terminated string)
     * @param value The integer value to store
     */
    void overlay_prefs_set_int(const char* key, int value);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_DEBUG_OVERLAY_PREFS_H */
