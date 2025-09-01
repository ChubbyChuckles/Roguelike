/* Validation panel wired to state_validation_manager APIs. */
#include "../core/integration/state_validation_manager.h"
#include "overlay_core.h"
#include "overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_validation(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("validation", "Validation", 820, 380, 360))
        return;

    overlay_label("Run content validators:");
    if (overlay_button("Validate All"))
    {
        /* Trigger a full validation run now */
        (void) rogue_validation_run_now(1);
    }

    RogueValidationStats st = {0};
    rogue_validation_get_stats(&st);
    char summary[160];
    snprintf(summary, sizeof summary,
             "runs=%llu sys=%llu skipped=%llu cross=%llu warn=%llu corrupt=%llu repairs=%llu/%llu",
             (unsigned long long) st.runs_completed, (unsigned long long) st.system_validations_run,
             (unsigned long long) st.system_validations_skipped_unchanged,
             (unsigned long long) st.cross_rule_runs, (unsigned long long) st.warnings,
             (unsigned long long) st.corruptions_detected,
             (unsigned long long) st.repairs_succeeded, (unsigned long long) st.repairs_attempted);
    overlay_label(summary);

    const RogueValidationEvent* evs = NULL;
    size_t ev_count = 0;
    if (rogue_validation_events_get(&evs, &ev_count) == 0 && evs && ev_count > 0)
    {
        int open = 1;
        if (overlay_tree_node("Recent Events", &open))
        {
            size_t start = (ev_count > 10) ? (ev_count - 10) : 0; /* show last 10 */
            for (size_t i = start; i < ev_count; ++i)
            {
                char line[192];
                snprintf(line, sizeof line, "#%llu sys=%d sev=%d code=%u msg=%s",
                         (unsigned long long) evs[i].seq, evs[i].system_id, evs[i].severity,
                         evs[i].code, evs[i].message);
                overlay_label(line);
            }
            overlay_tree_pop();
        }
    }
    overlay_end_panel();
}

void rogue_overlay_register_panel_validation(void)
{
    overlay_register_panel("validation", "Validation", panel_validation, NULL);
}

#endif
