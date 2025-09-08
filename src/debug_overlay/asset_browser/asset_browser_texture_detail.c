/* asset_browser_texture_detail.c - extracted from panels_asset_browser.c */
#include "asset_browser_texture_detail.h"
#include "../../asset/asset_manager.h"
#include "../../core/app/app_state.h"
#include "../../graphics/sprite.h"
#include "../../util/asset_dep.h"
#include "../overlay_core.h"
#include "../overlay_theme.h"
#include "../widgets/overlay_widgets.h"
#include "../widgets/overlay_widgets_internal.h"
#include "asset_browser_state.h"
#include "asset_browser_util.h"
#include <stdio.h>
#include <string.h>

#if defined(ROGUE_HAVE_SDL)
#include <SDL.h>
#endif

#define g_ab_state (*rogue_asset_browser_state())

/* Local layout shim (was static inside panels_asset_browser.c; needed after extraction). */
static void overlay_same_line(void) { /* no-op; overlay minimal layout */ }

void rogue_asset_browser_draw_texture_detail(const struct RogueAssetTexture* sel_tex,
                                             const struct RogueAssetManager* m,
                                             const char* active_filter)
{
    (void) active_filter;
    if (!sel_tex || !m)
        return;
    char line[256];
    snprintf(line, sizeof line, "Selected Texture: id=%s w=%d h=%d ref=%u fail=%d loaded=%d",
             sel_tex->id, sel_tex->width, sel_tex->height, sel_tex->ref_count,
             sel_tex->load_failed ? 1 : 0, sel_tex->sdl_texture ? 1 : 0);
    overlay_label(line);
    if (g_ab_state.compare_tex_a < 0)
        g_ab_state.compare_tex_a = -1;
    if (g_ab_state.compare_tex_b < 0)
        g_ab_state.compare_tex_b = -1;
    {
        int current_tex_index = rogue_asset_manager_find_by_id(sel_tex->id);
        if (overlay_button("Set Compare A"))
            g_ab_state.compare_tex_a = current_tex_index;
        overlay_same_line();
        if (overlay_button("Set Compare B"))
            g_ab_state.compare_tex_b = current_tex_index;
    }
    if (g_ab_state.compare_tex_a >= 0 || g_ab_state.compare_tex_b >= 0)
    {
        const struct RogueAssetTexture* ta =
            (g_ab_state.compare_tex_a >= 0 &&
             (uint32_t) g_ab_state.compare_tex_a < m->texture_count)
                ? &m->textures[g_ab_state.compare_tex_a]
                : NULL;
        const struct RogueAssetTexture* tb =
            (g_ab_state.compare_tex_b >= 0 &&
             (uint32_t) g_ab_state.compare_tex_b < m->texture_count)
                ? &m->textures[g_ab_state.compare_tex_b]
                : NULL;
        overlay_label("-- Comparison --");
        if (!ta || !tb)
        {
            overlay_label("Select two textures (Set Compare A/B) to view diff.");
        }
        else
        {
            char cline[192];
            int dw = ta->width - tb->width;
            int dh = ta->height - tb->height;
            double apx_a = (double) ta->width * (double) ta->height;
            double apx_b = (double) tb->width * (double) tb->height;
            double apx_ratio = (apx_b > 0.0) ? (apx_a / apx_b) : 0.0;
            snprintf(cline, sizeof cline, "A: %s (%dx%d)  B: %s (%dx%d)", ta->id, ta->width,
                     ta->height, tb->id, tb->width, tb->height);
            overlay_label(cline);
            snprintf(cline, sizeof cline, "Delta (A-B): w=%d h=%d  AreaRatio=%.2f", dw, dh,
                     apx_ratio);
            overlay_label(cline);
            if (ta->sdl_texture && tb->sdl_texture)
                overlay_label("(Preview deferred: scaled draw helper missing)");
            if (overlay_button("Clear Comparison"))
            {
                g_ab_state.compare_tex_a = -1;
                g_ab_state.compare_tex_b = -1;
            }
        }
    }
    {
        int tex_index = rogue_asset_manager_find_by_id(sel_tex->id);
        if (tex_index >= 0)
        {
            overlay_label("Tags:");
            const char* ttags[8];
            int tc = rogue_asset_manager_list_texture_tags(tex_index, ttags, 8);
            if (tc == 0)
                overlay_label("(none)");
            for (int ti = 0; ti < tc; ++ti)
                if (overlay_button(ttags[ti]))
                    rogue_asset_manager_remove_texture_tag(tex_index, ttags[ti]);
            if (overlay_input_text("Add Tag", g_ab_state.tag_input, sizeof g_ab_state.tag_input))
            {
            }
            if (overlay_button("+Tag") && g_ab_state.tag_input[0])
            {
                rogue_asset_manager_add_texture_tag(tex_index, g_ab_state.tag_input);
                g_ab_state.tag_input[0] = '\0';
            }
        }
    }
    if (g_ab_state.tex_zoom < 1)
        g_ab_state.tex_zoom = 1;
    overlay_label("Preview Controls:");
    if (overlay_button("Zoom+ ") && g_ab_state.tex_zoom < 16)
        g_ab_state.tex_zoom++;
    if (overlay_button("Zoom- ") && g_ab_state.tex_zoom > 1)
        g_ab_state.tex_zoom--;
    if (overlay_button("Reset View"))
    {
        g_ab_state.tex_zoom = 1;
        g_ab_state.pan_x = 0;
        g_ab_state.pan_y = 0;
    }
    if (overlay_button("Pan Up"))
        g_ab_state.pan_y -= 8;
    if (overlay_button("Pan Down"))
        g_ab_state.pan_y += 8;
    if (overlay_button("Pan Left"))
        g_ab_state.pan_x -= 8;
    if (overlay_button("Pan Right"))
        g_ab_state.pan_x += 8;
#if defined(ROGUE_HAVE_SDL)
    if (!sel_tex->sdl_texture && !sel_tex->load_failed)
    {
        if (overlay_button("Force Load Now"))
        {
            int idx = rogue_asset_manager_find_by_id(sel_tex->id);
            if (idx >= 0)
                rogue_asset_manager_ensure_texture_loaded(idx);
        }
    }
#endif
    {
        const char* dep_ids[32];
        int depc = rogue_asset_dep_get_deps(sel_tex->id, dep_ids, 32);
        if (depc > 0)
        {
            overlay_label("Deps:");
            for (int di = 0; di < depc; ++di)
                overlay_label(dep_ids[di]);
        }
    }
#if defined(ROGUE_HAVE_SDL)
    if (sel_tex->sdl_texture && sel_tex->width > 0 && sel_tex->height > 0)
    {
        int scale = g_ab_state.tex_zoom > 0 ? g_ab_state.tex_zoom : 1;
        RogueTexture wrap = {0};
        wrap.handle = (SDL_Texture*) sel_tex->sdl_texture;
        wrap.w = sel_tex->width;
        wrap.h = sel_tex->height;
        RogueSprite spr = {0};
        spr.tex = &wrap;
        spr.sw = wrap.w;
        spr.sh = wrap.h;
        overlay_label("Preview:");
        int px = g_ui.cur_x + g_ab_state.pan_x;
        int py = g_ui.cur_y + 2 + g_ab_state.pan_y;
        rogue_sprite_draw(&spr, px, py, scale);
        if (g_ab_state.sprite_grid_show && g_app.renderer)
        {
            int cw = g_ab_state.sprite_grid_cell_w > 0 ? g_ab_state.sprite_grid_cell_w : 32;
            int ch = g_ab_state.sprite_grid_cell_h > 0 ? g_ab_state.sprite_grid_cell_h : 32;
            if (cw < 4)
                cw = 4;
            if (ch < 4)
                ch = 4;
            int gw = spr.sw * scale;
            int gh = spr.sh * scale;
            const OverlayTheme* th_grid = overlay_theme_get();
            SDL_SetRenderDrawColor(g_app.renderer, th_grid->accent_1.r, th_grid->accent_1.g,
                                   th_grid->accent_1.b, 160);
            for (int vx = 0; vx <= spr.sw; vx += cw)
            {
                int x0 = px + vx * scale;
                SDL_RenderDrawLine(g_app.renderer, x0, py, x0, py + gh);
            }
            for (int hy = 0; hy <= spr.sh; hy += ch)
            {
                int y0 = py + hy * scale;
                SDL_RenderDrawLine(g_app.renderer, px, y0, px + gw, y0);
            }
        }
        g_ui.cur_y = py + spr.sh * scale + 4;
        overlay_checkbox("Show Grid", &g_ab_state.sprite_grid_show);
        if (g_ab_state.sprite_grid_cell_w <= 0)
            g_ab_state.sprite_grid_cell_w = 32;
        if (g_ab_state.sprite_grid_cell_h <= 0)
            g_ab_state.sprite_grid_cell_h = 32;
        overlay_slider_int("Cell W", &g_ab_state.sprite_grid_cell_w, 4, sel_tex->width);
        overlay_slider_int("Cell H", &g_ab_state.sprite_grid_cell_h, 4, sel_tex->height);
        if (overlay_button("Toggle Sprite Edit"))
        {
            g_ab_state.sprite_edit_mode = g_ab_state.sprite_edit_mode ? 0 : 1;
            g_ab_state.sprite_active_rect = -1;
        }
        if (g_ab_state.sprite_edit_mode)
        {
            const OverlayInputState* ist2 = overlay_input_get();
            if (ist2 && ist2->mouse_clicked)
            {
                int mx = ist2->mouse_x - px;
                int my = ist2->mouse_y - py;
                if (mx >= 0 && my >= 0 && mx < spr.sw * scale && my < spr.sh * scale)
                {
                    int sel = -1;
                    for (int ri = 0; ri < g_ab_state.sprite_rect_count; ++ri)
                    {
                        int rx = g_ab_state.sprite_rects[ri].x * scale;
                        int ry = g_ab_state.sprite_rects[ri].y * scale;
                        int rw = g_ab_state.sprite_rects[ri].w * scale;
                        int rh = g_ab_state.sprite_rects[ri].h * scale;
                        if (mx >= rx && my >= ry && mx < rx + rw && my < ry + rh)
                        {
                            sel = ri;
                            break;
                        }
                    }
                    if (sel >= 0)
                    {
                        g_ab_state.sprite_active_rect = sel;
                    }
                    else if (g_ab_state.sprite_rect_count < 64)
                    {
                        int cellw =
                            g_ab_state.sprite_grid_cell_w > 0 ? g_ab_state.sprite_grid_cell_w : 32;
                        int cellh =
                            g_ab_state.sprite_grid_cell_h > 0 ? g_ab_state.sprite_grid_cell_h : 32;
                        int gx = mx / scale / cellw * cellw;
                        int gy = my / scale / cellh * cellh;
                        g_ab_state.sprite_rects[g_ab_state.sprite_rect_count].x = gx;
                        g_ab_state.sprite_rects[g_ab_state.sprite_rect_count].y = gy;
                        g_ab_state.sprite_rects[g_ab_state.sprite_rect_count].w = cellw;
                        g_ab_state.sprite_rects[g_ab_state.sprite_rect_count].h = cellh;
                        g_ab_state.sprite_active_rect = g_ab_state.sprite_rect_count;
                        g_ab_state.sprite_rect_count++;
                    }
                }
            }
            for (int ri = 0; ri < g_ab_state.sprite_rect_count; ++ri)
            {
                int rx = px + g_ab_state.sprite_rects[ri].x * scale;
                int ry = py + g_ab_state.sprite_rects[ri].y * scale;
                int rw = g_ab_state.sprite_rects[ri].w * scale;
                int rh = g_ab_state.sprite_rects[ri].h * scale;
                Uint8 cr = 0, cg = 255, cb = 0, ca = 200;
                if (ri == g_ab_state.sprite_active_rect)
                {
                    cr = 255;
                    cg = 200;
                    cb = 0;
                }
                SDL_SetRenderDrawColor(g_app.renderer, cr, cg, cb, ca);
                SDL_RenderDrawLine(g_app.renderer, rx, ry, rx + rw, ry);
                SDL_RenderDrawLine(g_app.renderer, rx, ry, rx, ry + rh);
                SDL_RenderDrawLine(g_app.renderer, rx + rw, ry, rx + rw, ry + rh);
                SDL_RenderDrawLine(g_app.renderer, rx, ry + rh, rx + rw, ry + rh);
            }
            if (overlay_button("Delete Active Rect") && g_ab_state.sprite_active_rect >= 0)
            {
                int del = g_ab_state.sprite_active_rect;
                if (del < g_ab_state.sprite_rect_count - 1)
                    g_ab_state.sprite_rects[del] =
                        g_ab_state.sprite_rects[g_ab_state.sprite_rect_count - 1];
                g_ab_state.sprite_rect_count--;
                g_ab_state.sprite_active_rect = -1;
            }
            if (overlay_button("Export Sprite Coords (stub)"))
                overlay_label("(export stub – future JSON write)");
            overlay_label("Anim Frames (Phase 3 initial):");
            if (overlay_button("Add Frame") && g_ab_state.sprite_active_rect >= 0 &&
                g_ab_state.anim_frame_count <
                    (int) (sizeof(g_ab_state.anim_frames) / sizeof(g_ab_state.anim_frames[0])))
            {
                int idx = g_ab_state.anim_frame_count++;
                g_ab_state.anim_frames[idx].rect_index = g_ab_state.sprite_active_rect;
                g_ab_state.anim_frames[idx].duration_ms = 120;
                g_ab_state.anim_active_frame = idx;
            }
            if (g_ab_state.anim_active_frame >= g_ab_state.anim_frame_count)
                g_ab_state.anim_active_frame = g_ab_state.anim_frame_count - 1;
            if (g_ab_state.anim_active_frame < -1)
                g_ab_state.anim_active_frame = -1;
            if (g_ab_state.anim_active_frame >= 0)
            {
                if (overlay_button("Frame Dur +") &&
                    g_ab_state.anim_frames[g_ab_state.anim_active_frame].duration_ms < 2000)
                    g_ab_state.anim_frames[g_ab_state.anim_active_frame].duration_ms += 20;
                if (overlay_button("Frame Dur -") &&
                    g_ab_state.anim_frames[g_ab_state.anim_active_frame].duration_ms > 20)
                    g_ab_state.anim_frames[g_ab_state.anim_active_frame].duration_ms -= 20;
                if (overlay_button("Move Up") && g_ab_state.anim_active_frame > 0)
                {
                    int a = g_ab_state.anim_active_frame;
                    int tmp_rect = g_ab_state.anim_frames[a - 1].rect_index;
                    int tmp_dur = g_ab_state.anim_frames[a - 1].duration_ms;
                    g_ab_state.anim_frames[a - 1] = g_ab_state.anim_frames[a];
                    g_ab_state.anim_frames[a].rect_index = tmp_rect;
                    g_ab_state.anim_frames[a].duration_ms = tmp_dur;
                    g_ab_state.anim_active_frame = a - 1;
                }
                if (overlay_button("Move Down") &&
                    g_ab_state.anim_active_frame + 1 < g_ab_state.anim_frame_count)
                {
                    int a = g_ab_state.anim_active_frame;
                    int tmp_rect = g_ab_state.anim_frames[a + 1].rect_index;
                    int tmp_dur = g_ab_state.anim_frames[a + 1].duration_ms;
                    g_ab_state.anim_frames[a + 1] = g_ab_state.anim_frames[a];
                    g_ab_state.anim_frames[a].rect_index = tmp_rect;
                    g_ab_state.anim_frames[a].duration_ms = tmp_dur;
                    g_ab_state.anim_active_frame = a + 1;
                }
                if (overlay_button("Delete Frame"))
                {
                    int del = g_ab_state.anim_active_frame;
                    if (del < g_ab_state.anim_frame_count - 1)
                        g_ab_state.anim_frames[del] =
                            g_ab_state.anim_frames[g_ab_state.anim_frame_count - 1];
                    g_ab_state.anim_frame_count--;
                    g_ab_state.anim_active_frame = -1;
                }
            }
            if (overlay_button("Export Sprite Data (stub)"))
                overlay_label("(sprite+anim export stub – future JSON write)");
        }
        if (g_ab_state.sprite_rect_count > 0)
        {
            overlay_label("Rects:");
            for (int ri = 0; ri < g_ab_state.sprite_rect_count && ri < 8; ++ri)
            {
                char rbuf[64];
                snprintf(rbuf, sizeof rbuf, "%c #%d x=%d y=%d w=%d h=%d",
                         ri == g_ab_state.sprite_active_rect ? '*' : ' ', ri,
                         g_ab_state.sprite_rects[ri].x, g_ab_state.sprite_rects[ri].y,
                         g_ab_state.sprite_rects[ri].w, g_ab_state.sprite_rects[ri].h);
                overlay_label(rbuf);
            }
        }
        if (g_ab_state.anim_frame_count > 0)
        {
            overlay_label("Frames:");
            for (int fi = 0; fi < g_ab_state.anim_frame_count && fi < 12; ++fi)
            {
                int ar = g_ab_state.anim_frames[fi].rect_index;
                int dur = g_ab_state.anim_frames[fi].duration_ms;
                char fbuf[80];
                snprintf(fbuf, sizeof fbuf, "%c #%d rect=%d dur=%dms",
                         fi == g_ab_state.anim_active_frame ? '*' : ' ', fi, ar, dur);
                if (overlay_button(fbuf))
                    g_ab_state.anim_active_frame = fi;
            }
        }
    }
#endif
#if defined(ROGUE_HAVE_SDL)
    overlay_label("Batch / Export (Phase3 slice):");
    static int rs_w = 64;
    static int rs_h = 64;
    if (rs_w < 4)
        rs_w = 4;
    if (rs_h < 4)
        rs_h = 4;
    overlay_slider_int("Resize W", &rs_w, 4, sel_tex->width * 4);
    overlay_slider_int("Resize H", &rs_h, 4, sel_tex->height * 4);
    if (overlay_button("Create Resize Variant"))
    {
        int tindex = rogue_asset_manager_find_by_id(sel_tex->id);
        if (tindex >= 0)
        {
            int ridx = rogue_asset_manager_resize_texture_variant(tindex, rs_w, rs_h, 0);
            overlay_label(ridx >= 0 ? "(variant created)" : "(resize failed)");
        }
    }
    if (overlay_button("Resize In-Place"))
    {
        int tindex = rogue_asset_manager_find_by_id(sel_tex->id);
        if (tindex >= 0)
        {
            int ridx = rogue_asset_manager_resize_texture_variant(tindex, rs_w, rs_h, 1);
            overlay_label(ridx >= 0 ? "(resized)" : "(resize failed)");
        }
    }
    static char export_path[260];
    if (!export_path[0])
        snprintf(export_path, sizeof export_path, "export_%s.bmp", sel_tex->id);
    if (overlay_input_text("Export Path", export_path, sizeof export_path))
        export_path[sizeof export_path - 1] = '\0';
    if (overlay_button("Export BMP"))
    {
        int tindex = rogue_asset_manager_find_by_id(sel_tex->id);
        if (tindex >= 0)
        {
            if (rogue_asset_manager_export_texture_bmp(tindex, export_path))
                overlay_label("(export ok)");
            else
                overlay_label("(export failed)");
        }
    }
#endif
}
