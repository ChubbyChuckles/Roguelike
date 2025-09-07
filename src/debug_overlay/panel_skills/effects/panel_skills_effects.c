/* Orchestrator after modular refactor: delegates to specialized sub-modules. */
#include "panel_skills_effects.h"
#include "../../../core/skills/skill_debug.h"
#include "../../../graphics/effect_spec.h"
#include "../../widgets/overlay_widgets.h"
#include "../shared/panel_skills_shared.h"
#include "effects_node_graph.h"
#include "effects_node_list.h"
#include "effects_palette.h"
#include "effects_tree_editor.h"
#include "effects_validation.h"
#include <string.h>

#if ROGUE_ENABLE_DEBUG_OVERLAY
void panel_skills_draw_effects(int sel)
{
    const char* overrides_path = panel_skills_overrides_path();
    overlay_label("Effects");

    /* Fetch experimental tree data first */
    struct RogueSkillEffectTreeNodeDebug tree_nodes[8];
    int tree_count = 0;
    int tree_supported = (rogue_skill_debug_get_effect_tree(sel, tree_nodes, &tree_count) == 0);
    static int tree_mode = 0; /* 0 legacy, 1 tree */

    if (tree_supported)
    {
        overlay_checkbox("Use Effect Tree (experimental)", &tree_mode);
        if (tree_mode && tree_count == 0)
        {
            tree_count = 1;
            tree_nodes[0].effect_spec_id = -1;
            tree_nodes[0].delay_ms = 0.0f;
            tree_nodes[0].duration_ms = 0.0f;
            tree_nodes[0].repeat_count = 1;
            tree_nodes[0].repeat_interval_ms = 0.0f;
            tree_nodes[0].require_player_health_below_pct = 0;
            tree_nodes[0].parent_index = -1;
        }
    }

    /* Legacy flat nodes */
    int primary_id = -1;
    struct RogueSkillEffectNode nodes[3];
    int node_count = 3;
    memset(nodes, 0, sizeof nodes);
    for (int i = 0; i < 3; ++i)
        nodes[i].effect_spec_id = -1;
    if (!tree_mode && rogue_skill_debug_get_effects(sel, &primary_id, nodes, &node_count) != 0)
    {
        overlay_label("Failed to fetch effects for skill.");
        node_count = 0;
        primary_id = -1;
    }

    int changed = 0;

    /* Tree editor (only when enabled & supported) */
    if (tree_mode && tree_supported)
        changed |= effects_tree_editor_draw(sel, overrides_path, tree_nodes, &tree_count);

    /* Validation (always run for legacy node array) */
    changed |= effects_validation_draw(&primary_id, nodes, node_count);

    /* Effect palette: assignments to primary / nodes */
    changed |= effects_palette_draw(&primary_id, nodes, node_count);

    /* Legacy node graph visualization/editor */
    changed |= effects_node_graph_editor_draw(sel, &primary_id, nodes, node_count);

    /* Basic node list editor + primary slider + per-node sliders */
    changed |= effects_node_list_editor_draw(&primary_id, nodes, &node_count);

    /* Persist modifications */
    if (changed)
    {
        if (!tree_mode)
            (void) rogue_skill_debug_set_effects(sel, primary_id, nodes, node_count);
        if (tree_mode && tree_supported)
            (void) rogue_skill_debug_set_effect_tree(sel, tree_nodes, tree_count);
        (void) rogue_skill_debug_save_overrides(overrides_path);
        panel_skills_refresh_validation();
    }
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
