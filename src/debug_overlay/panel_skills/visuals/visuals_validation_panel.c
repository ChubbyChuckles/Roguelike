/* Skill Visuals Asset Validation Feedback Panel
   Provides per-skill diagnostic of referenced sprite assets with lightweight thumbnail previews.
   Focus: missing files, load failures (corrupt/unsupported), dimension sanity (0 or extreme size).
*/
#include "visuals_internal.h"

#include <stdio.h>
#include <string.h>

struct validation_thumb
{
    RogueTexture tex;   /* loaded texture (if any) */
    char path[256];     /* path it corresponds to */
    int load_attempted; /* 1 after first attempt */
    int load_failed;    /* 1 if last attempt failed */
    int missing;        /* file not found */
};

static struct
{
    int show;
    struct validation_thumb cast;
    struct validation_thumb projectile;
    struct validation_thumb impact;
    struct validation_thumb aoe;
    int expanded;     /* foldout for details */
    int auto_refresh; /* reload every frame (when previewing rapid edits) */
} g_val_panel;

static void vp_reset_thumb(struct validation_thumb* t)
{
#ifdef ROGUE_HAVE_SDL
    if (t->tex.w || t->tex.h) /* heuristic: if dimensions set, destroy */
    {
        rogue_texture_destroy(&t->tex);
    }
#else
    if (t->tex.w || t->tex.h)
    {
        rogue_texture_destroy(&t->tex);
    }
#endif
    memset(t, 0, sizeof *t);
}

static void vp_try_load(struct validation_thumb* t, const char* path)
{
    if (!path || !*path)
    {
        vp_reset_thumb(t);
        return;
    }
    if (strncmp(t->path, path, sizeof t->path) == 0 && t->load_attempted &&
        !g_val_panel.auto_refresh)
        return; /* up to date */
    vp_reset_thumb(t);
    strncpy(t->path, path, sizeof t->path - 1);
    t->path[sizeof t->path - 1] = '\0';
    t->load_attempted = 1;
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        t->missing = 1;
        return;
    }
    fclose(f);
    if (!rogue_texture_load(&t->tex, path))
    {
        t->load_failed = 1; /* corrupt/unsupported */
        return;
    }
}

static void vp_draw_thumb(struct validation_thumb* t, const char* label)
{
    char status[256];
    if (!t->load_attempted)
    {
        snprintf(status, sizeof status, "%s: (unset)", label);
        overlay_label(status);
        return;
    }
    if (t->missing)
    {
        snprintf(status, sizeof status, "%s: %s (MISSING)", label, t->path);
        overlay_label(status);
        return;
    }
    /* treat zero-dimension after attempted load as failure */
    if (t->load_failed || (t->tex.w == 0 && t->tex.h == 0))
    {
        snprintf(status, sizeof status, "%s: %s (CORRUPT/UNSUPPORTED)", label, t->path);
        overlay_label(status);
        return;
    }
    int bad_dims = (t->tex.w <= 0 || t->tex.h <= 0 || t->tex.w > 4096 || t->tex.h > 4096);
    snprintf(status, sizeof status, "%s: %s (%dx%d)%s", label, t->path, t->tex.w, t->tex.h,
             bad_dims ? " (DIM ERR)" : "");
    overlay_label(status);
#ifdef ROGUE_HAVE_SDL
    if (t->tex.w > 0 && t->tex.h > 0)
    {
        RogueSprite spr = {0};
        spr.tex = &t->tex;
        spr.sw = t->tex.w;
        spr.sh = t->tex.h;
        int thumb_w = 72;
        int scale = spr.sw > 0 ? thumb_w / spr.sw : 1;
        if (scale < 1)
            scale = 1;
        if (scale > 6)
            scale = 6;
        int px = g_ui.cur_x;
        int py = g_ui.cur_y + 4;
        rogue_sprite_draw(&spr, px, py, scale);
        g_ui.cur_y = py + spr.sh * scale + 4;
    }
#endif
}

void rogue_visuals_validation_panel(RogueSkillVisualParams* vis)
{
    /* Panel only meaningful when overlay enabled; guard runtime usage */
#if ROGUE_ENABLE_DEBUG_OVERLAY
    if (overlay_button("Open Asset Validation Feedback"))
        g_val_panel.show = 1;
    if (!g_val_panel.show)
        return;
    overlay_begin_panel("Asset Validation Feedback", 1420, 60, 360);
    overlay_label("Thumbnails + diagnostics for skill visual sprite assets.");
    overlay_checkbox("Auto Refresh", &g_val_panel.auto_refresh);
    if (overlay_button("Refresh Now"))
    {
        vp_reset_thumb(&g_val_panel.cast);
        vp_reset_thumb(&g_val_panel.projectile);
        vp_reset_thumb(&g_val_panel.impact);
        vp_reset_thumb(&g_val_panel.aoe);
    }
    vp_try_load(&g_val_panel.cast, vis->cast_sprite_sheet);
    vp_try_load(&g_val_panel.projectile, vis->projectile_sprite);
    vp_try_load(&g_val_panel.impact, vis->impact_sprite);
    vp_try_load(&g_val_panel.aoe, vis->aoe_sprite);
    vp_draw_thumb(&g_val_panel.cast, "Cast");
    vp_draw_thumb(&g_val_panel.projectile, "Projectile");
    vp_draw_thumb(&g_val_panel.impact, "Impact");
    vp_draw_thumb(&g_val_panel.aoe, "AoE");
    overlay_label("Status Legend: MISSING=file absent, CORRUPT=failed to load, DIM ERR=invalid or "
                  "extreme dimensions (>4096).");
    if (overlay_button("Close"))
        g_val_panel.show = 0;
    overlay_end_panel();
#else
    (void) vis; /* suppress unused warning if compiled without overlay */
#endif
}
