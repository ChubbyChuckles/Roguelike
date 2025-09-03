#include "../../core/app/app_state.h"
#include "../../core/world/map_debug.h"
#include "../overlay_core.h"
#include "../overlay_icon.h"
#include "../overlay_input.h"
#include "../overlay_theme.h"
#include "../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_map_editor(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("map", "Map Editor", 1190, 10, 360))
        return;
    /* Split view: left controls, right preview/inspector (placeholder) */
    int left_w = 360;
    overlay_splitter_begin("map.split", &left_w, 260, 720);
    static int brush_radius = 1;
    static int brush_mode = 0;                     /* 0 = square, 1 = rect */
    static int erase_mode = 0;                     /* when set, paint EMPTY */
    static int tile_val = 1;                       /* default GRASS */
    static int rx0 = 0, ry0 = 0, rx1 = 7, ry1 = 7; /* rect inputs */
    static char path_buf[128] = "build/map.json";
    static int show_colliders = 0;
    static int show_heat = 0;
    static int show_autotile_hints = 0;

    if (brush_radius < 0)
        brush_radius = 0;
    const char* tile_names[ROGUE_TILE_MAX] = {
        "EMPTY",       "WATER",        "GRASS",      "FOREST",   "MOUNTAIN",
        "CAVE_WALL",   "CAVE_FLOOR",   "RIVER",      "SWAMP",    "SNOW",
        "RIVER_DELTA", "RIVER_WIDE",   "LAVA",       "ORE_VEIN", "BRIDGE_HINT",
        "STRUCT_WALL", "STRUCT_FLOOR", "DNG_ENTR",   "DNG_WALL", "DNG_FLOOR",
        "DNG_DOOR",    "DNG_LOCKED",   "DNG_SECRET", "DNG_TRAP", "DNG_KEY"};

    int max_tile = ROGUE_TILE_MAX - 1;
    if (tile_val < 0)
        tile_val = 0;
    if (tile_val > max_tile)
        tile_val = max_tile;

    overlay_slider_int("Tile ID", &tile_val, 0, max_tile);
    {
        char lbl[96];
        const char* nm = (tile_val >= 0 && tile_val < ROGUE_TILE_MAX) ? tile_names[tile_val] : "?";
        snprintf(lbl, sizeof lbl, "Selected: [%d] %s", tile_val, nm);
        overlay_label(lbl);
    }
    overlay_checkbox("Erase (paint EMPTY)", &erase_mode);
    {
        const char* modes[] = {"Square", "Rect"};
        (void) overlay_combo("Brush Mode", &brush_mode, modes, 2);
    }
    if (brush_mode == 0)
    {
        overlay_slider_int("Square Radius", &brush_radius, 0, 32);
        if (overlay_columns_begin(2, NULL))
        {
            if (overlay_button("Paint at Center"))
            {
                int cx = g_app.world_map.width / 2;
                int cy = g_app.world_map.height / 2;
                unsigned char v = (unsigned char) (erase_mode ? 0 : tile_val);
                (void) rogue_map_debug_brush_square(cx, cy, brush_radius, v);
            }
            overlay_next_column();
            if (overlay_button("Paint at Player"))
            {
                int px = (int) g_app.player.base.pos.x;
                int py = (int) g_app.player.base.pos.y;
                if (px < 0)
                    px = 0;
                if (py < 0)
                    py = 0;
                if (px >= g_app.world_map.width)
                    px = g_app.world_map.width - 1;
                if (py >= g_app.world_map.height)
                    py = g_app.world_map.height - 1;
                unsigned char v = (unsigned char) (erase_mode ? 0 : tile_val);
                (void) rogue_map_debug_brush_square(px, py, brush_radius, v);
            }
            overlay_columns_end();
        }
    }
    else
    {
        overlay_slider_int("x0", &rx0, 0,
                           (g_app.world_map.width > 0) ? g_app.world_map.width - 1 : 0);
        overlay_slider_int("y0", &ry0, 0,
                           (g_app.world_map.height > 0) ? g_app.world_map.height - 1 : 0);
        overlay_slider_int("x1", &rx1, 0,
                           (g_app.world_map.width > 0) ? g_app.world_map.width - 1 : 0);
        overlay_slider_int("y1", &ry1, 0,
                           (g_app.world_map.height > 0) ? g_app.world_map.height - 1 : 0);
        if (overlay_button("Paint Rect"))
        {
            unsigned char v = (unsigned char) (erase_mode ? 0 : tile_val);
            (void) rogue_map_debug_brush_rect(rx0, ry0, rx1, ry1, v);
        }
    }

    /* Undo/Redo row */
    if (overlay_columns_begin(3, NULL))
    {
        if (overlay_icon_button("Undo", OVERLAY_ICON_UNDO))
            rogue_map_debug_undo();
        overlay_next_column();
        if (overlay_icon_button("Redo", OVERLAY_ICON_REDO))
            rogue_map_debug_redo();
        overlay_next_column();
        if (overlay_button("Clear History"))
            rogue_map_debug_undo_clear();
        overlay_columns_end();
    }

    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_button("Pick Under Player"))
        {
            int px = (int) g_app.player.base.pos.x;
            int py = (int) g_app.player.base.pos.y;
            if (px >= 0 && py >= 0 && px < g_app.world_map.width && py < g_app.world_map.height)
                tile_val = (int) g_app.world_map.tiles[py * g_app.world_map.width + px];
        }
        overlay_next_column();
        /* reserved for future quick actions */
        overlay_columns_end();
    }

    {
        static int adv_open = 0;
        if (overlay_tree_node("Advanced", &adv_open))
        {
            if (overlay_button("Clear (EMPTY)"))
            {
                (void) rogue_map_debug_brush_rect(0, 0, g_app.world_map.width - 1,
                                                  g_app.world_map.height - 1, 0);
            }
            overlay_checkbox("Show Colliders", &show_colliders);
            overlay_checkbox("Show Heatmap (A* cost)", &show_heat);
            overlay_checkbox("Show Autotile Hints", &show_autotile_hints);
            overlay_tree_pop();
        }
    }

    overlay_input_text("Map JSON Path", path_buf, sizeof path_buf);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_icon_button("Save JSON", OVERLAY_ICON_SAVE))
        {
            int rc = rogue_map_debug_save_json(path_buf);
            char msg[64];
            snprintf(msg, sizeof msg, "save rc=%d", rc);
            overlay_label(msg);
        }
        overlay_next_column();
        if (overlay_icon_button("Load JSON", OVERLAY_ICON_PLAY))
        {
            int rc = rogue_map_debug_load_json(path_buf);
            char msg[64];
            snprintf(msg, sizeof msg, "load rc=%d", rc);
            overlay_label(msg);
        }
        overlay_columns_end();
    }
    /* Right pane: mini-map style preview with brush ghost and overlays */
    overlay_next_column();
    {
        int ts = g_app.tile_size ? g_app.tile_size : 32;
        const int vis_w = (g_app.viewport_w > 0) ? g_app.viewport_w : 640;
        const int vis_h = (g_app.viewport_h > 0) ? g_app.viewport_h : 360;
        char row[160];
        snprintf(row, sizeof row, "View: cam(%.1f,%.1f) tiles(%dx%d) ts=%d", g_app.cam_x,
                 g_app.cam_y, g_app.world_map.width, g_app.world_map.height, ts);
        overlay_label(row);
        /* Brush ghost textual preview (tile coords under cursor)
            Use struct tag to avoid relying on typedef visibility across builds. */
        const struct OverlayInputState* in = overlay_input_get();
        if (in)
        {
            int mx = (int) in->mouse_x;
            int my = (int) in->mouse_y;
            int tx = (int) ((g_app.cam_x + mx) / (float) ts);
            int ty = (int) ((g_app.cam_y + my) / (float) ts);
            if (tx >= 0 && ty >= 0 && tx < g_app.world_map.width && ty < g_app.world_map.height)
            {
                char gbuf[96];
                if (brush_mode == 0)
                    snprintf(gbuf, sizeof gbuf, "Ghost: Square @ (%d,%d) r=%d tile=%d", tx, ty,
                             brush_radius, erase_mode ? 0 : tile_val);
                else
                    snprintf(gbuf, sizeof gbuf, "Ghost: Rect (%d,%d)-(%d,%d) tile=%d", rx0, ry0,
                             rx1, ry1, erase_mode ? 0 : tile_val);
                overlay_label(gbuf);
            }
        }
#ifdef ROGUE_HAVE_SDL
        /* Visual gizmos (guarded) */
        if (g_app.renderer)
        {
            const OverlayTheme* th = overlay_theme_get();
            /* Fallback colors if theme not initialized for some reason */
            const OverlayColor accent1 = th ? th->accent_1 : (OverlayColor){255, 64, 64, 160};
            const OverlayColor accent2 = th ? th->accent_2 : (OverlayColor){200, 200, 200, 160};
            /* Brush ghost rectangle */
            int tx = -1, ty = -1;
            if (in)
            {
                tx = (int) ((g_app.cam_x + in->mouse_x) / (float) ts);
                ty = (int) ((g_app.cam_y + in->mouse_y) / (float) ts);
            }
            if (brush_mode == 0 && tx >= 0 && ty >= 0)
            {
                int x0 = (int) ((tx - brush_radius) * ts - g_app.cam_x);
                int y0 = (int) ((ty - brush_radius) * ts - g_app.cam_y);
                int w = (2 * brush_radius + 1) * ts;
                int h = w;
                SDL_Rect r = {x0, y0, w, h};
                SDL_SetRenderDrawColor(g_app.renderer, accent2.r, accent2.g, accent2.b, 64);
                SDL_RenderFillRect(g_app.renderer, &r);
                SDL_SetRenderDrawColor(g_app.renderer, accent2.r, accent2.g, accent2.b, 160);
                SDL_RenderDrawRect(g_app.renderer, &r);
            }
            else if (brush_mode == 1)
            {
                int x0 = (int) (rx0 * ts - g_app.cam_x);
                int y0 = (int) (ry0 * ts - g_app.cam_y);
                int x1 = (int) ((rx1 + 1) * ts - g_app.cam_x);
                int y1 = (int) ((ry1 + 1) * ts - g_app.cam_y);
                SDL_Rect r = {x0, y0, x1 - x0, y1 - y0};
                SDL_SetRenderDrawColor(g_app.renderer, accent2.r, accent2.g, accent2.b, 64);
                SDL_RenderFillRect(g_app.renderer, &r);
                SDL_SetRenderDrawColor(g_app.renderer, accent2.r, accent2.g, accent2.b, 160);
                SDL_RenderDrawRect(g_app.renderer, &r);
            }
            /* Colliders overlay: draw red outlines on blocked tiles in view */
            if (show_colliders)
            {
                int first_tx = (int) (g_app.cam_x / ts);
                int first_ty = (int) (g_app.cam_y / ts);
                int vis_tx = vis_w / ts + 2;
                int vis_ty = vis_h / ts + 2;
                int last_tx = first_tx + vis_tx;
                int last_ty = first_ty + vis_ty;
                if (last_tx > g_app.world_map.width)
                    last_tx = g_app.world_map.width;
                if (last_ty > g_app.world_map.height)
                    last_ty = g_app.world_map.height;
                SDL_SetRenderDrawColor(g_app.renderer, accent1.r, accent1.g, accent1.b, 160);
                for (int y = first_ty; y < last_ty; ++y)
                {
                    for (int x = first_tx; x < last_tx; ++x)
                    {
                        /* reuse nav API to classify */
                        extern int rogue_nav_is_blocked(int tx, int ty);
                        if (rogue_nav_is_blocked(x, y))
                        {
                            SDL_Rect r = {(int) (x * ts - g_app.cam_x),
                                          (int) (y * ts - g_app.cam_y), ts, ts};
                            SDL_RenderDrawRect(g_app.renderer, &r);
                        }
                    }
                }
            }
            /* Pathfinding heatmap: modulate alpha by tile cost */
            if (show_heat)
            {
                extern float rogue_nav_tile_cost(int tx, int ty);
                int first_tx = (int) (g_app.cam_x / ts);
                int first_ty = (int) (g_app.cam_y / ts);
                int vis_tx = vis_w / ts + 2;
                int vis_ty = vis_h / ts + 2;
                int last_tx = first_tx + vis_tx;
                int last_ty = first_ty + vis_ty;
                if (last_tx > g_app.world_map.width)
                    last_tx = g_app.world_map.width;
                if (last_ty > g_app.world_map.height)
                    last_ty = g_app.world_map.height;
                for (int y = first_ty; y < last_ty; ++y)
                {
                    for (int x = first_tx; x < last_tx; ++x)
                    {
                        float c = rogue_nav_tile_cost(x, y); /* ~1 normal, >1 slower */
                        if (c < 0.99f)
                            c = 0.99f;
                        if (c > 4.0f)
                            c = 4.0f;
                        int a = (int) (50 + (c - 1.0f) * 60.0f); /* 50..230 approx */
                        if (a < 50)
                            a = 50;
                        if (a > 230)
                            a = 230;
                        /* Use accent_2 hue with varying alpha for readability */
                        SDL_SetRenderDrawColor(g_app.renderer, accent2.r, accent2.g, accent2.b, a);
                        SDL_Rect r = {(int) (x * ts - g_app.cam_x), (int) (y * ts - g_app.cam_y),
                                      ts, ts};
                        SDL_RenderFillRect(g_app.renderer, &r);
                    }
                }
            }
            /* Autotiling hints: draw small corner ticks based on neighbor similarity */
            if (show_autotile_hints)
            {
                int first_tx = (int) (g_app.cam_x / ts);
                int first_ty = (int) (g_app.cam_y / ts);
                int vis_tx = vis_w / ts + 2;
                int vis_ty = vis_h / ts + 2;
                int last_tx = first_tx + vis_tx;
                int last_ty = first_ty + vis_ty;
                if (last_tx > g_app.world_map.width)
                    last_tx = g_app.world_map.width;
                if (last_ty > g_app.world_map.height)
                    last_ty = g_app.world_map.height;
                SDL_SetRenderDrawColor(g_app.renderer, accent2.r, accent2.g, accent2.b, 200);
                for (int y = first_ty; y < last_ty; ++y)
                {
                    for (int x = first_tx; x < last_tx; ++x)
                    {
                        unsigned char t = g_app.world_map.tiles[y * g_app.world_map.width + x];
                        int sameR =
                            (x + 1 < g_app.world_map.width) &&
                            (g_app.world_map.tiles[y * g_app.world_map.width + (x + 1)] == t);
                        int sameL =
                            (x - 1 >= 0) &&
                            (g_app.world_map.tiles[y * g_app.world_map.width + (x - 1)] == t);
                        int sameU =
                            (y - 1 >= 0) &&
                            (g_app.world_map.tiles[(y - 1) * g_app.world_map.width + x] == t);
                        int sameD =
                            (y + 1 < g_app.world_map.height) &&
                            (g_app.world_map.tiles[(y + 1) * g_app.world_map.width + x] == t);
                        int sx = (int) (x * ts - g_app.cam_x);
                        int sy = (int) (y * ts - g_app.cam_y);
                        if (!sameU)
                            SDL_RenderDrawLine(g_app.renderer, sx + 2, sy + 2, sx + ts - 2, sy + 2);
                        if (!sameD)
                            SDL_RenderDrawLine(g_app.renderer, sx + 2, sy + ts - 2, sx + ts - 2,
                                               sy + ts - 2);
                        if (!sameL)
                            SDL_RenderDrawLine(g_app.renderer, sx + 2, sy + 2, sx + 2, sy + ts - 2);
                        if (!sameR)
                            SDL_RenderDrawLine(g_app.renderer, sx + ts - 2, sy + 2, sx + ts - 2,
                                               sy + ts - 2);
                    }
                }
            }
        }
#endif /* ROGUE_HAVE_SDL */
    }
    overlay_splitter_end();
    overlay_end_panel();
}

void rogue_overlay_register_panel_map(void)
{
    overlay_register_panel("map", "Map Editor", panel_map_editor, NULL);
}

#endif
