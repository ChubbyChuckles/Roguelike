/* asset_browser_asset_list.c - extracted listing/filter UI from panels_asset_browser.c
   Responsibilities:
     - Iterate over textures/audio/json/shader cached lists based on active tab
     - Apply wildcard + tag filters
     - Render rows as buttons (selection) or labels (selected)
     - Maintain g_ab_state.selected_row as a flattened index across visible types
   Notes:
     - Detail panes (texture/audio preview, sprite editor, etc.) remain in the panel for now.
     - Wildcard helper centralized in asset_browser_util.c (rogue_ab_match_wildcard_ci).
*/

#include "asset_browser_asset_list.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

#include "../../asset/asset_manager.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "asset_browser_state.h"
#include "asset_browser_util.h"

#include <string.h>

#define g_ab_state (*rogue_asset_browser_state())

void rogue_asset_browser_draw_asset_list(const char* filter)
{
    RogueAssetManager* m = rogue_asset_manager_instance();
    if (!m || !m->initialized)
        return;
    if (!filter)
        filter = "";
    /* Local buffers */
    char line[256];
    int shown = 0;
    int limit = 300;                /* soft cap to avoid runaway */
    int tab = g_ab_state.tab_index; /* 0=All 1=Textures 2=Audio 3=JSON 4=Shaders */
    int current_row_index = 0;      /* flattened row for selection */

/* PASS_FILTER: matches wildcard against path/id (case-insensitive) */
#define PASS_FILTER(txt, id)                                                                       \
    (!(filter)[0] || rogue_ab_match_wildcard_ci((txt), (filter)) ||                                \
     rogue_ab_match_wildcard_ci((id), (filter)))

    /* Textures (unless tab explicitly audio/json/shader) */
    if (tab == 0 || tab == 1)
    {
        uint32_t i;
        for (i = 0; i < m->texture_count && limit > 0; ++i)
        {
            const RogueAssetTexture* t = &m->textures[i];
            if (!PASS_FILTER(t->path, t->id))
                continue;
            if (g_ab_state.tag_filter[0])
            {
                int tex_index_tag = (int) i;
                if (!rogue_asset_manager_has_texture_tag(tex_index_tag, g_ab_state.tag_filter))
                    continue;
            }
            if (tab != 0 && tab != 1)
                break; /* defensive */
            snprintf(line, sizeof line, "T%03u %s w=%d h=%d ref=%u%s%s", i, t->id, t->width,
                     t->height, t->ref_count, t->load_failed ? " FAIL" : "",
                     t->sdl_texture ? " *" : "");
            if (g_ab_state.selected_row == current_row_index)
                overlay_label(line);
            else if (overlay_button(line))
                g_ab_state.selected_row = current_row_index;
            current_row_index++;
            shown++;
            limit--;
        }
    }
    /* Audio */
    if (tab == 0 || tab == 2)
    {
        uint32_t i;
        for (i = 0; i < m->audio_count && limit > 0; ++i)
        {
            const RogueAssetAudio* a = &m->audio[i];
            if (!PASS_FILTER(a->path, a->id))
                continue;
            if (g_ab_state.tag_filter[0])
            {
                int audio_index_tag = (int) i;
                if (!rogue_asset_manager_has_audio_tag(audio_index_tag, g_ab_state.tag_filter))
                    continue;
            }
            if (tab != 0 && tab != 2)
                break;
            snprintf(line, sizeof line, "A%03u %s ref=%u%s%s", i, a->id, a->ref_count,
                     a->load_failed ? " FAIL" : "", a->sdl_chunk ? " *" : "");
            if (g_ab_state.selected_row == current_row_index)
                overlay_label(line);
            else if (overlay_button(line))
                g_ab_state.selected_row = current_row_index;
            current_row_index++;
            shown++;
            limit--;
        }
    }
    /* JSON */
    if (tab == 0 || tab == 3)
    {
        int i;
        for (i = 0; i < g_ab_state.json_count && limit > 0; ++i)
        {
            const char* path = g_ab_state.json_files[i].path;
            if (!PASS_FILTER(path, path))
                continue;
            snprintf(line, sizeof line, "J %s", path);
            overlay_label(line); /* non-selectable in current model */
            shown++;
            limit--;
        }
    }
    /* Shaders */
    if (tab == 0 || tab == 4)
    {
        int i;
        for (i = 0; i < g_ab_state.shader_count && limit > 0; ++i)
        {
            const char* path = g_ab_state.shader_files[i].path;
            if (!PASS_FILTER(path, path))
                continue;
            snprintf(line, sizeof line, "S %s", path);
            overlay_label(line);
            shown++;
            limit--;
        }
    }

#undef PASS_FILTER
    (void) shown; /* placeholder for future scroll virtualization */
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
