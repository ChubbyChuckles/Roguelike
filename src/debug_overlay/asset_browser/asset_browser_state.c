#include "asset_browser_state.h"
#include <string.h>

static AssetBrowserEnhancedState g_asset_browser_state; /* zero-init */

AssetBrowserEnhancedState* rogue_asset_browser_state(void) { return &g_asset_browser_state; }
