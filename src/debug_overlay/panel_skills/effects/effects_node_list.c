#include "effects_node_list.h"
#include "../../../graphics/effect_spec.h"
#include "../../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY
int effects_node_list_editor_draw(int* primary_id, struct RogueSkillEffectNode* nodes,
                                  int* node_count)
{
    int changed = 0;
    changed |= overlay_slider_int("Primary EffectSpec ID", primary_id, -1, 4096);
    if (*primary_id > 0)
    {
        const RogueEffectSpec* s = rogue_effect_get(*primary_id);
        overlay_label(s ? "Primary: OK" : "Primary: INVALID id");
    }
    else
    {
        overlay_label("Primary: (unset)");
    }
    int display_count = *node_count;
    if (overlay_slider_int("Additional Nodes (0..3)", &display_count, 0, 3))
    {
        if (display_count < 0)
            display_count = 0;
        if (display_count > 3)
            display_count = 3;
        if (display_count > *node_count)
        {
            for (int i = *node_count; i < display_count; ++i)
            {
                nodes[i].effect_spec_id = -1;
                nodes[i].delay_ms = 0.0f;
                nodes[i].duration_ms = 0.0f;
                nodes[i].repeat_count = 0;
                nodes[i].repeat_interval_ms = 0.0f;
                nodes[i].require_player_health_below_pct = 0;
            }
        }
        *node_count = display_count;
        changed = 1;
    }
    for (int i = 0; i < *node_count; ++i)
    {
        char hdr[64];
        snprintf(hdr, sizeof hdr, "Node %d", i + 1);
        overlay_label(hdr);
        changed |= overlay_slider_int("  EffectSpec ID", &nodes[i].effect_spec_id, -1, 4096);
        if (nodes[i].effect_spec_id > 0)
        {
            const RogueEffectSpec* s = rogue_effect_get(nodes[i].effect_spec_id);
            overlay_label(s ? "  Effect: OK" : "  Effect: INVALID id");
        }
        else
        {
            overlay_label("  Effect: (unset)");
        }
        changed |= overlay_slider_float("  Delay (ms)", &nodes[i].delay_ms, 0.0f, 10000.0f);
        changed |= overlay_slider_float("  Duration (ms)", &nodes[i].duration_ms, 0.0f, 60000.0f);
        changed |= overlay_slider_int("  Repeat Count", &nodes[i].repeat_count, 0, 100);
        changed |= overlay_slider_float("  Repeat Interval (ms)", &nodes[i].repeat_interval_ms,
                                        0.0f, 10000.0f);
        int hp_gate = nodes[i].require_player_health_below_pct;
        changed |= overlay_slider_int("  HP Below % (gate)", &hp_gate, 0, 100);
        nodes[i].require_player_health_below_pct = (unsigned char) hp_gate;
    }
    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
