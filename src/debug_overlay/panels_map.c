#include "../core/app/app_state.h"
#include "../core/world/map_debug.h"
#include "overlay_core.h"
#include "overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_map_editor(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("map", "Map Editor", 1190, 10, 360))
        return;
    static int brush_radius = 1;
    static int brush_mode = 0;                     /* 0 = square, 1 = rect */
    static int erase_mode = 0;                     /* when set, paint EMPTY */
    static int tile_val = 1;                       /* default GRASS */
    static int rx0 = 0, ry0 = 0, rx1 = 7, ry1 = 7; /* rect inputs */
    static char path_buf[128] = "build/map.json";

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
        if (overlay_button("Fill Entire Map"))
        {
            unsigned char v = (unsigned char) (erase_mode ? 0 : tile_val);
            (void) rogue_map_debug_brush_rect(0, 0, g_app.world_map.width - 1,
                                              g_app.world_map.height - 1, v);
        }
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
            overlay_tree_pop();
        }
    }

    overlay_input_text("Map JSON Path", path_buf, sizeof path_buf);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_button("Save JSON"))
        {
            int rc = rogue_map_debug_save_json(path_buf);
            char msg[64];
            snprintf(msg, sizeof msg, "save rc=%d", rc);
            overlay_label(msg);
        }
        overlay_next_column();
        if (overlay_button("Load JSON"))
        {
            int rc = rogue_map_debug_load_json(path_buf);
            char msg[64];
            snprintf(msg, sizeof msg, "load rc=%d", rc);
            overlay_label(msg);
        }
        overlay_columns_end();
    }
    overlay_end_panel();
}

void rogue_overlay_register_panel_map(void)
{
    overlay_register_panel("map", "Map Editor", panel_map_editor, NULL);
}

#endif
