#include "effects_validation.h"
#include "../../../graphics/effect_spec.h"
#include "../../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY
int effects_validation_draw(int* primary_id, struct RogueSkillEffectNode* nodes, int node_count)
{
    int changed = 0;
    int local_errors = 0;
    char line[192];
    if (*primary_id > 0 && rogue_effect_get(*primary_id) == NULL)
    {
        snprintf(line, sizeof line, "ERROR: primary effect_spec_id=%d is invalid", *primary_id);
        overlay_label(line);
        if (overlay_button("Fix: Clear Primary"))
        {
            *primary_id = -1;
            changed = 1;
        }
        ++local_errors;
    }
    for (int ni = 0; ni < node_count; ++ni)
    {
        const int eid = nodes[ni].effect_spec_id;
        if (eid > 0 && rogue_effect_get(eid) == NULL)
        {
            snprintf(line, sizeof line, "ERROR: node %d effect_spec_id=%d invalid", ni + 1, eid);
            overlay_label(line);
            if (overlay_button("Fix: Clear Node"))
            {
                nodes[ni].effect_spec_id = -1;
                changed = 1;
            }
            ++local_errors;
        }
        if (nodes[ni].duration_ms < 0.0f)
        {
            snprintf(line, sizeof line, "ERROR: node %d duration_ms < 0", ni + 1);
            overlay_label(line);
            if (overlay_button("Fix: Set duration 0"))
            {
                nodes[ni].duration_ms = 0.0f;
                changed = 1;
            }
            ++local_errors;
        }
        if (nodes[ni].repeat_count < 0 || nodes[ni].repeat_count > 32)
        {
            snprintf(line, sizeof line, "ERROR: node %d repeat_count out of range (0..32)", ni + 1);
            overlay_label(line);
            if (overlay_button("Fix: Clamp 0..32"))
            {
                if (nodes[ni].repeat_count < 0)
                    nodes[ni].repeat_count = 0;
                if (nodes[ni].repeat_count > 32)
                    nodes[ni].repeat_count = 32;
                changed = 1;
            }
            ++local_errors;
        }
        if (nodes[ni].repeat_count == 0 && nodes[ni].duration_ms > 0.0f &&
            nodes[ni].repeat_interval_ms <= 0.0f)
        {
            snprintf(line, sizeof line, "ERROR: node %d duration set but repeat_interval_ms <= 0",
                     ni + 1);
            overlay_label(line);
            if (overlay_button("Fix: Set interval 1000ms"))
            {
                nodes[ni].repeat_interval_ms = 1000.0f;
                changed = 1;
            }
            ++local_errors;
        }
        if (nodes[ni].require_player_health_below_pct > 100)
        {
            snprintf(line, sizeof line, "ERROR: node %d HP gate > 100%% (value=%u)", ni + 1,
                     (unsigned) nodes[ni].require_player_health_below_pct);
            overlay_label(line);
            if (overlay_button("Fix: Clamp to 100%"))
            {
                nodes[ni].require_player_health_below_pct = 100;
                changed = 1;
            }
            ++local_errors;
        }
    }
    if (local_errors == 0)
        overlay_label("Local check: OK");
    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
