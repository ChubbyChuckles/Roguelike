/* hot_reload.h - Phase M3.3 hot reload infrastructure (Complete)
 *
 * Provides a lightweight registry of reloadable asset/config loaders.
 * Initial milestone: manual trigger API (force reload by id) used by tests.
 * Future (remaining M3.3 work): file timestamp polling + selective module
 * integration (skills, attacks, loot, world gen) and error surfacing.
 */
#ifndef ROGUE_UTIL_HOT_RELOAD_H
#define ROGUE_UTIL_HOT_RELOAD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RogueHotReloadFn)(const char* path, void* user_data);

/* Register a reload handler.
 * id: unique identifier (stored by copy, truncated if needed)
 * path: file path to reload (stored by copy)
 * fn: callback invoked on force or detected change
 * user_data: opaque pointer passed to callback
 * Returns 0 on success, -1 on failure (capacity / bad args / duplicate id).
 */
int rogue_hot_reload_register(const char* id, const char* path, RogueHotReloadFn fn, void* user_data);

/* Force invoke the reload handler for id (ignores timestamps).
 * Returns 0 on success, -1 if id not found. */
int rogue_hot_reload_force(const char* id);

/* Poll for file content hash changes and invoke handlers for changed entries.
 * Returns number of handlers invoked. */
int rogue_hot_reload_tick(void);

/* Reset registry (intended for tests). */
void rogue_hot_reload_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_UTIL_HOT_RELOAD_H */
