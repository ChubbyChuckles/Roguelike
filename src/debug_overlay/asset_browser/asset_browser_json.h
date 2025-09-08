#ifndef ROGUE_ASSET_BROWSER_JSON_H
#define ROGUE_ASSET_BROWSER_JSON_H

#include "asset_browser_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Initialize undo stack using current editor buffer (if any) */
    void rogue_asset_browser_json_undo_init(void);
    /* Push current editor buffer onto undo stack (with compaction/redo reset). */
    void rogue_asset_browser_json_undo_push_current(void);
    int rogue_asset_browser_json_undo_can_undo(void);
    int rogue_asset_browser_json_undo_can_redo(void);
    void rogue_asset_browser_json_undo_do_undo(void);
    void rogue_asset_browser_json_undo_do_redo(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_BROWSER_JSON_H */
