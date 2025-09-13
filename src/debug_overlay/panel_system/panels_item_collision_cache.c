/* panels_item_collision_cache.c - Item Collision Cache advisory panel (read-only)
 */
#include "../../game/item_collision_cache.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_item_collision_cache(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("item_collision_cache", "Item Collision Cache", 10, 350, 360))
        return;

    RogueItemCollisionCacheAdvisory adv;
    rogue_item_collision_cache_get_advisory_ex(&adv, /*clamp_min_current_limits=*/1);

    int cur_cap = 0;
    size_t cur_mem = 0;
    rogue_item_collision_cache_get_limits(&cur_cap, &cur_mem);

    char line[160];
    snprintf(line, sizeof line, "Alive: %d  Window: %d", adv.alive_entries, adv.recent_window);
    overlay_label(line);

    snprintf(line, sizeof line, "Percentiles (bytes): p50=%zu  p90=%zu  p99=%zu", adv.p50_bytes,
             adv.p90_bytes, adv.p99_bytes);
    overlay_label(line);

    snprintf(line, sizeof line, "Current Limits: entries=%d  mem=%zu MiB", cur_cap, cur_mem);
    overlay_label(line);

    snprintf(line, sizeof line, "Advisory: entries=%d  mem=%zu MiB (clamped)",
             adv.recommended_max_entries, adv.recommended_max_memory_mb);
    overlay_label(line);

    overlay_label("Note: Read-only advisory. Use API to apply during maintenance.");

    overlay_end_panel();
}

void rogue_overlay_register_panel_item_collision_cache(void)
{
    overlay_register_panel("item_collision_cache", "Item Collision Cache",
                           panel_item_collision_cache, NULL);
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
