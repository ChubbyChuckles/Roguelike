/* Asset Browser Memory Profiler (extracted from panels_asset_browser.c)
   Provides lightweight per-frame approximate texture memory statistics. */
#ifndef ROGUE_ASSET_BROWSER_MEMORY_PROFILER_H
#define ROGUE_ASSET_BROWSER_MEMORY_PROFILER_H

#include "asset/asset_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void rogue_asset_browser_draw_memory_profiler(RogueAssetManager* m);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_BROWSER_MEMORY_PROFILER_H */
