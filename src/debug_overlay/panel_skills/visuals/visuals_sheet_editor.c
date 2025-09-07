#include "visuals_internal.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

void rogue_visuals_draw_sheet_editor(RogueSkillVisualParams* vis, int* vchanged)
{
    struct rogue_visuals_preview_state* ps = rogue_visuals_preview_state_get();
    static int show = 0;
    if (overlay_button("Sprite Sheet Editor"))
        show = !show;
    if (!show)
        return;
    overlay_begin_panel("Sprite Sheet Editor", 600, 60, 440);
    overlay_label("Grid Overlay & Frame Picker");
    overlay_label("Left click: select frame | Right click-drag: adjust grid (W/H)");

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
        overlay_label("(No sprite loaded – assign a sprite first)");
    }
    else
    {
        static int selected_frame = -1;
        (void) vchanged;
        int editing_grid = (ps->preview_target == PREV_CAST);
        int gw = editing_grid ? vis->grid_width : 0;
        int gh = editing_grid ? vis->grid_height : 0;
        if (editing_grid == 0)
            overlay_label("(Grid editing only for Cast sheet – switch Preview Target)");

        /* Auto infer grid if unset and dimensions divide evenly (quality-of-life) */
        if (editing_grid && gw <= 0 && gh <= 0 && ps->tex.w > 0 && ps->tex.h > 0)
        {
            if (ps->tex.w % 64 == 0 && ps->tex.h % 64 == 0)
            {
                vis->grid_width = ps->tex.w / 64;
                vis->grid_height = ps->tex.h / 64;
                gw = vis->grid_width;
                gh = vis->grid_height;
            }
        }
        int px = g_ui.cur_x;
        int py = g_ui.cur_y + 4;
        int avail_w = g_ui.width - 8;
        if (avail_w < 32)
            avail_w = 32;
        int scale = avail_w / (ps->tex.w > 0 ? ps->tex.w : 1);
        if (scale < 1)
            scale = 1;
        if (scale > 6)
            scale = 6;
        RogueSprite sheet = {0};
        sheet.tex = &ps->tex;
        sheet.sw = ps->tex.w;
        sheet.sh = ps->tex.h;
        rogue_sprite_draw(&sheet, px, py, scale);

        /* Overlay grid lines & pick logic */
        if (editing_grid && gw > 0 && gh > 0)
        {
            int cell_w = (ps->tex.w / gw) * scale;
            int cell_h = (ps->tex.h / gh) * scale;
            int mx = overlay_mouse_x();
            int my = overlay_mouse_y();
            int hover_frame = -1;
            if (mx >= px && my >= py && mx < px + ps->tex.w * scale && my < py + ps->tex.h * scale)
            {
                int rel_x = mx - px;
                int rel_y = my - py;
                int cx = rel_x / cell_w;
                int cy = rel_y / cell_h;
                if (cx >= 0 && cx < gw && cy >= 0 && cy < gh)
                    hover_frame = cy * gw + cx;
            }
            if (hover_frame >= 0 && overlay_mouse_pressed_left())
            {
                selected_frame = hover_frame;
            }
            if (overlay_mouse_down_right())
            {
                /* Drag to change grid (rough heuristic): horizontal drag -> gw, vertical -> gh */
                static int start_mx = 0, start_my = 0, base_gw = 0, base_gh = 0, adjusting = 0;
                if (!adjusting)
                {
                    adjusting = 1;
                    start_mx = mx;
                    start_my = my;
                    base_gw = gw > 0 ? gw : 1;
                    base_gh = gh > 0 ? gh : 1;
                }
                int dx = mx - start_mx;
                int dy = my - start_my;
                int new_gw = base_gw + dx / 64;
                int new_gh = base_gh + dy / 64;
                if (new_gw < 1)
                    new_gw = 1;
                if (new_gh < 1)
                    new_gh = 1;
                if (new_gw != vis->grid_width || new_gh != vis->grid_height)
                {
                    vis->grid_width = new_gw;
                    vis->grid_height = new_gh;
                    *vchanged = 1;
                }
            }
            else
            {
                /* end adjust cycle */
            }
            /* Draw selected outline info */
            char info[128];
            snprintf(info, sizeof info, "Grid %dx%d  Selected: %d", vis->grid_width,
                     vis->grid_height, selected_frame);
            overlay_label(info);
            if (selected_frame >= 0)
            {
                if (selected_frame + 1 > vis->frame_count)
                {
                    if (overlay_button("Grow Frame Count to Selected+1"))
                    {
                        vis->frame_count = selected_frame + 1;
                        *vchanged = 1;
                    }
                }
            }
            if (overlay_button("Set Frame Count = Grid Cells"))
            {
                int total = vis->grid_width * vis->grid_height;
                if (total != vis->frame_count)
                {
                    vis->frame_count = total;
                    *vchanged = 1;
                }
            }
        }
        else
        {
            overlay_label("(Set grid width/height for frame picking)");
        }
        g_ui.cur_y = py + ps->tex.h * scale + 10;
    }
    if (overlay_button("Close"))
        show = 0;
    overlay_end_panel();
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
