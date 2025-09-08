#ifndef ROGUE_ASSET_BROWSER_DIR_H
#define ROGUE_ASSET_BROWSER_DIR_H

#include "asset_browser_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void rogue_asset_browser_dir_init_if_needed(void);
    void rogue_asset_browser_dir_refresh(void);
    void rogue_asset_browser_dir_parent(char* path);
    void rogue_asset_browser_dir_join(char* out, size_t cap, const char* a, const char* b);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_BROWSER_DIR_H */
