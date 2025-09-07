#include "visuals_internal.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

void rogue_visuals_draw_sheet_editor(RogueSkillVisualParams* vis, int* vchanged)
{
    struct rogue_visuals_preview_state* ps = rogue_visuals_preview_state_get();
    static int show = 0;
    if (overlay_button("Open Sprite Sheet Editor"))
        show = 1;
    if (!show)
        return;
    overlay_begin_panel("Sprite Sheet Editor", 600, 60, 420);
    overlay_label("Grid Overlay & Frame Picker (prototype)");

    const char* path = NULL;
    if (ps->preview_target == PREV_CAST)
        path = vis->cast_sprite_sheet;
    else if (ps->preview_target == PREV_PROJECTILE)
        path = vis->projectile_sprite;
    else if (ps->preview_target == PREV_IMPACT)
        path = vis->impact_sprite;
    else if (ps->preview_target == PREV_AOE)
        path = vis->aoe_sprite;

    if (!path || !*path || !ps->loaded)
    {
        overlay_label("(No sprite loaded – assign a Cast sheet or target sprite)");
    }
    else
    {
        static int selected_frame = 0;
        (void) vchanged;
        int gw = (ps->preview_target == PREV_CAST) ? vis->grid_width : 0;
        int gh = (ps->preview_target == PREV_CAST) ? vis->grid_height : 0;
        if (ps->preview_target != PREV_CAST)
            overlay_label("(Grid editing only supported for Cast sheet in this slice)");
        if (gw > 0 && gh > 0 && ps->tex.w > 0 && ps->tex.h > 0)
        {
            int px = g_ui.cur_x;
            int py = g_ui.cur_y + 4;
            int avail_w = g_ui.width - 8;
            if (avail_w < 32)
                avail_w = 32;
            int scale = avail_w / (ps->tex.w > 0 ? ps->tex.w : 1);
            if (scale < 1)
                scale = 1;
            if (scale > 4)
                scale = 4;
            RogueSprite sheet = {0};
            sheet.tex = &ps->tex;
            sheet.sw = ps->tex.w;
            sheet.sh = ps->tex.h;
            rogue_sprite_draw(&sheet, px, py, scale);
            int draw_h = ps->tex.h * scale;
            g_ui.cur_y = py + draw_h + 6;
            char info[96];
            snprintf(info, sizeof info, "Selected Frame: %d / %d", selected_frame, gw * gh - 1);
            overlay_label(info);
            if (overlay_button("Apply Frame Count = Selected Index+1"))
            {
                if (selected_frame + 1 > vis->frame_count)
                {
                    vis->frame_count = selected_frame + 1;
                    *vchanged = 1;
                }
            }
        }
        else
            overlay_label("(Set Grid Width/Height on Cast sheet to enable grid editor)");
    }
    if (overlay_button("Close Editor"))
        show = 0;
    overlay_end_panel();
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
