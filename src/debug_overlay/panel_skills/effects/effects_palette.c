#include "effects_palette.h"
#include "../../../game/buffs.h"
#include "../../../graphics/effect_spec.h"
#include "../../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static int palette_effect_categories_local(const RogueEffectSpec* es)
{
    if (!es)
        return 0;
    int cats = 0;
    if (es->debuff)
        cats |= ROGUE_BUFF_CAT_OFFENSIVE;
    if (es->kind == ROGUE_EFFECT_STAT_BUFF)
    {
        if (es->buff_type >= 0 && es->buff_type < ROGUE_BUFF_MAX)
            cats |= (int) rogue_buffs_type_categories((RogueBuffType) es->buff_type);
        else
            cats |= ROGUE_BUFF_CAT_UTILITY;
    }
    if (es->kind == ROGUE_EFFECT_AURA)
        cats |= ROGUE_BUFF_CAT_OFFENSIVE;
    return cats;
}

int effects_palette_draw(int* primary_id, struct RogueSkillEffectNode* nodes, int node_count)
{
    int changed = 0;
    static int palette_open = 1;
    static char eff_filter[64] = "";
    static int assign_target = -1; /* -1=primary else node index */
    static int eff_selected = -1;
    static int kind_filter = -1;
    static int debuff_only = 0;
    static int cat_filter = 0;
    overlay_checkbox("Show EffectSpec Palette", &palette_open);
    if (!palette_open)
        return 0;

    overlay_input_text("Filter (id substring)", eff_filter, sizeof eff_filter);
    overlay_slider_int("Assign To: -1=Primary, 0..2 Node", &assign_target, -1, 2);
    overlay_slider_int("Kind Filter (-1 any, 0 buff, 1 dot, 2 aura)", &kind_filter, -1, 2);
    overlay_checkbox("Debuff only", &debuff_only);
    overlay_label("Category Filter (toggle to OR):");
    int widths[4] = {120, 120, 120, 120};
    overlay_columns_begin(4, widths);
    int t_off = (cat_filter & ROGUE_BUFF_CAT_OFFENSIVE) != 0;
    int t_def = (cat_filter & ROGUE_BUFF_CAT_DEFENSIVE) != 0;
    int t_mov = (cat_filter & ROGUE_BUFF_CAT_MOVEMENT) != 0;
    int t_utl = (cat_filter & ROGUE_BUFF_CAT_UTILITY) != 0;
    (void) overlay_checkbox("Offensive", &t_off);
    overlay_next_column();
    (void) overlay_checkbox("Defensive", &t_def);
    overlay_next_column();
    (void) overlay_checkbox("Movement", &t_mov);
    overlay_next_column();
    (void) overlay_checkbox("Utility", &t_utl);
    overlay_columns_end();
    cat_filter = 0;
    if (t_off)
        cat_filter |= ROGUE_BUFF_CAT_OFFENSIVE;
    if (t_def)
        cat_filter |= ROGUE_BUFF_CAT_DEFENSIVE;
    if (t_mov)
        cat_filter |= ROGUE_BUFF_CAT_MOVEMENT;
    if (t_utl)
        cat_filter |= ROGUE_BUFF_CAT_UTILITY;

    int ec = rogue_effect_count();
    const char* headers[] = {"ID", "Kind", "Debuff", "Dur"};
    int sort_col = 0, sort_dir = 0; /* (sorting ignored currently) */
    int tmp_sel = eff_selected;
    if (overlay_table_begin("effect_palette", headers, 4, &sort_col, &sort_dir, NULL))
    {
        char id_s[16], kind_s[8], deb_s[8], dur_s[16];
        for (int i = 0; i < ec; ++i)
        {
            const RogueEffectSpec* es = rogue_effect_get(i);
            if (!es)
                continue;
            snprintf(id_s, sizeof id_s, "%d", i);
            if (eff_filter[0] != '\0')
            {
                const char* p = id_s;
                const char* f = eff_filter;
                const char* hit = NULL;
                for (; *p && !hit; ++p)
                {
                    const char* p2 = p;
                    const char* f2 = f;
                    while (*p2 && *f2 && *p2 == *f2)
                    {
                        ++p2;
                        ++f2;
                    }
                    if (*f2 == '\0')
                        hit = p;
                }
                if (!hit)
                    continue;
            }
            if (kind_filter >= 0 && (int) es->kind != kind_filter)
                continue;
            if (debuff_only && es->debuff == 0)
                continue;
            if (cat_filter != 0)
            {
                int cats = palette_effect_categories_local(es);
                if ((cats & cat_filter) == 0)
                    continue;
            }
            snprintf(kind_s, sizeof kind_s, "%u", (unsigned) es->kind);
            snprintf(deb_s, sizeof deb_s, "%u", (unsigned) es->debuff);
            snprintf(dur_s, sizeof dur_s, "%.0f", es->duration_ms);
            const char* cells[] = {id_s, kind_s, deb_s, dur_s};
            (void) overlay_table_row(cells, 4, i, &tmp_sel);
        }
        overlay_table_end();
    }
    eff_selected = tmp_sel;
    if (overlay_button("Assign Selected"))
    {
        if (eff_selected >= 0)
        {
            if (assign_target < 0)
            {
                *primary_id = eff_selected;
                changed = 1;
            }
            else if (assign_target < node_count)
            {
                nodes[assign_target].effect_spec_id = eff_selected;
                changed = 1;
            }
        }
    }
    if (overlay_button("Clear Nodes"))
    {
        for (int i = 0; i < node_count; ++i)
        {
            nodes[i].effect_spec_id = -1;
            nodes[i].delay_ms = 0.0f;
            nodes[i].duration_ms = 0.0f;
            nodes[i].repeat_count = 0;
            nodes[i].repeat_interval_ms = 0.0f;
            nodes[i].require_player_health_below_pct = 0;
        }
        changed = 1;
    }
    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
