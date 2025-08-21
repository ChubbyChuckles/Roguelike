/* hud_layout.c - UI Phase 6.1 HUD layout spec loader implementation */
#include "hud_layout.h"
#include "util/kv_parser.h"
#include <stdio.h>
#include <string.h>

static RogueHUDLayout g_hud_layout;

static void apply_defaults(void)
{
    g_hud_layout.health.x = 6;
    g_hud_layout.health.y = 4;
    g_hud_layout.health.w = 200;
    g_hud_layout.health.h = 10;
    g_hud_layout.mana.x = 6;
    g_hud_layout.mana.y = g_hud_layout.health.y + g_hud_layout.health.h + 6;
    g_hud_layout.mana.w = 200;
    g_hud_layout.mana.h = 8;
    g_hud_layout.xp.x = 6;
    g_hud_layout.xp.y = g_hud_layout.mana.y + g_hud_layout.mana.h + 6;
    g_hud_layout.xp.w = 200;
    g_hud_layout.xp.h = 6;
    g_hud_layout.level_text_x = g_hud_layout.health.x + g_hud_layout.health.w + 8;
    g_hud_layout.level_text_y = g_hud_layout.health.y;
    g_hud_layout.loaded = 0;
}

void rogue_hud_layout_reset_defaults(void) { apply_defaults(); }

const RogueHUDLayout* rogue_hud_layout(void)
{
    if (g_hud_layout.health.w == 0)
        apply_defaults();
    return &g_hud_layout;
}

static int parse_bar(const char* value, RogueHUDBarRect* out)
{
    if (!value || !out)
        return 0;
    int x = 0, y = 0, w = 0, h = 0;
#if defined(_MSC_VER)
    if (sscanf_s(value, "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
#else
    if (sscanf(value, "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
#endif
    {
        if (w < 1)
            w = 1;
        if (h < 1)
            h = 1;
        out->x = x;
        out->y = y;
        out->w = w;
        out->h = h;
        return 1;
    }
    return 0;
}
static int parse_xy(const char* value, int* x, int* y)
{
    if (!value || !x || !y)
        return 0;
    int vx = 0, vy = 0;
#if defined(_MSC_VER)
    if (sscanf_s(value, "%d,%d", &vx, &vy) == 2)
#else
    if (sscanf(value, "%d,%d", &vx, &vy) == 2)
#endif
    {
        *x = vx;
        *y = vy;
        return 1;
    }
    return 0;
}

int rogue_hud_layout_load(const char* path)
{
    if (g_hud_layout.health.w == 0)
        apply_defaults();
    RogueKVFile kv;
    if (!rogue_kv_load_file(path, &kv))
    {
        /* attempt ../ fallback */
        char alt[256];
        snprintf(alt, sizeof alt, "../%s", path);
        if (!rogue_kv_load_file(alt, &kv))
            return 0; /* leave defaults */
    }
    int cursor = 0;
    RogueKVEntry e;
    RogueKVError err;
    int any = 0;
    while (rogue_kv_next(&kv, &cursor, &e, &err))
    {
        if (strcmp(e.key, "health_bar") == 0)
        {
            if (parse_bar(e.value, &g_hud_layout.health))
                any = 1;
        }
        else if (strcmp(e.key, "mana_bar") == 0)
        {
            if (parse_bar(e.value, &g_hud_layout.mana))
                any = 1;
        }
        else if (strcmp(e.key, "xp_bar") == 0)
        {
            if (parse_bar(e.value, &g_hud_layout.xp))
                any = 1;
        }
        else if (strcmp(e.key, "level_text") == 0)
        {
            if (parse_xy(e.value, &g_hud_layout.level_text_x, &g_hud_layout.level_text_y))
                any = 1;
        }
    }
    rogue_kv_free(&kv);
    if (any)
        g_hud_layout.loaded = 1;
    return any;
}
