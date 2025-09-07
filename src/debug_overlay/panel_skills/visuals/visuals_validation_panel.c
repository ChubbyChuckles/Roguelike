/* Skill Visuals Asset Validation Feedback Panel
   Provides per-skill diagnostic of referenced sprite assets with lightweight thumbnail previews.
   Focus: missing files, load failures (corrupt/unsupported), dimension sanity (0 or extreme size).
*/
#include "../../../core/skills/skill_asset_validation.h"
#include "visuals_internal.h"

#include <stdio.h>
#include <string.h>

struct validation_thumb
{
    RogueTexture tex;
    char path[256];
    int load_attempted;
    int load_failed;
    int missing;
    int dim_err;
    int ext_warn;
};

static struct
{
    int show;
    struct validation_thumb cast;
    struct validation_thumb projectile;
    struct validation_thumb impact;
    struct validation_thumb aoe;
    int expanded;
    int auto_refresh;
    int cast_suggest_gw, cast_suggest_gh, cast_suggest_fc, cast_has_suggestion;
} g_val_panel;

/* Forward declaration for hot-reload dependency change callback */
static int rogue_validation_panel_dep_changed(const char* path, void* user);

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
        return;
    vp_reset_thumb(t);
    strncpy(t->path, path, sizeof t->path - 1);
    t->path[sizeof t->path - 1] = '\0';
    t->load_attempted = 1;
    int missing = 0, load_failed = 0, dim_err = 0, w = 0, h = 0, ext_warn = 0;
    rogue_skill_asset_validate(path, &missing, &load_failed, &dim_err, &w, &h, &ext_warn);
    t->missing = missing;
    t->load_failed = load_failed;
    t->dim_err = dim_err;
    t->ext_warn = ext_warn;
    if (!missing && !load_failed)
    {
        if (rogue_texture_load(&t->tex, path))
        {
            if (w)
                t->tex.w = w;
            if (h)
                t->tex.h = h;
        }
        else
            t->load_failed = 1;
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
    int bad_dims =
        (t->dim_err || t->tex.w <= 0 || t->tex.h <= 0 || t->tex.w > 4096 || t->tex.h > 4096);
    snprintf(status, sizeof status, "%s: %s (%dx%d)%s%s", label, t->path, t->tex.w, t->tex.h,
             bad_dims ? " (DIM ERR)" : "", t->ext_warn ? " (EXT WARN)" : "");
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
    if (g_val_panel.cast.load_attempted && !g_val_panel.cast.missing &&
        !g_val_panel.cast.load_failed)
    {
        g_val_panel.cast_has_suggestion = rogue_visuals_infer_grid(
            g_val_panel.cast.tex.w, g_val_panel.cast.tex.h, &g_val_panel.cast_suggest_gw,
            &g_val_panel.cast_suggest_gh, &g_val_panel.cast_suggest_fc);
        if (g_val_panel.cast_has_suggestion)
        {
            char hint[128];
            snprintf(hint, sizeof hint, "Suggested Grid: %dx%d (%d frames)",
                     g_val_panel.cast_suggest_gw, g_val_panel.cast_suggest_gh,
                     g_val_panel.cast_suggest_fc);
            overlay_label(hint);
            if (overlay_button("Apply Suggested Grid"))
            {
                vis->grid_width = g_val_panel.cast_suggest_gw;
                vis->grid_height = g_val_panel.cast_suggest_gh;
                vis->frame_count = g_val_panel.cast_suggest_fc;
            }
        }
    }
    if (overlay_button("Normalize Paths (\\\\ -> /)"))
    {
        char* paths[] = {vis->cast_sprite_sheet, vis->projectile_sprite, vis->impact_sprite,
                         vis->aoe_sprite};
        for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
            for (char* p = paths[i]; *p; ++p)
                if (*p == '\\')
                    *p = '/';
    }
    if (overlay_button("Inject Placeholder For Missing"))
    {
        const char* placeholder = "assets/placeholder.png"; /* expected existing generic image */
        if (g_val_panel.cast.missing)
            strncpy(vis->cast_sprite_sheet, placeholder, sizeof vis->cast_sprite_sheet - 1);
        if (g_val_panel.projectile.missing)
            strncpy(vis->projectile_sprite, placeholder, sizeof vis->projectile_sprite - 1);
        if (g_val_panel.impact.missing)
            strncpy(vis->impact_sprite, placeholder, sizeof vis->impact_sprite - 1);
        if (g_val_panel.aoe.missing)
            strncpy(vis->aoe_sprite, placeholder, sizeof vis->aoe_sprite - 1);
    }
    if (overlay_button("Bulk Assign Cast → All Empty"))
    {
        if (vis->cast_sprite_sheet[0])
        {
            if (!vis->projectile_sprite[0])
                strncpy(vis->projectile_sprite, vis->cast_sprite_sheet,
                        sizeof vis->projectile_sprite - 1);
            if (!vis->impact_sprite[0])
                strncpy(vis->impact_sprite, vis->cast_sprite_sheet, sizeof vis->impact_sprite - 1);
            if (!vis->aoe_sprite[0])
                strncpy(vis->aoe_sprite, vis->cast_sprite_sheet, sizeof vis->aoe_sprite - 1);
        }
    }
    if (overlay_button("Poll Hot-Reload (deps)"))
    {
        extern int rogue_skill_asset_dep_poll_changes(int (*on_change)(const char*, void*), void*);
        rogue_skill_asset_dep_poll_changes(rogue_validation_panel_dep_changed, NULL);
    }
    overlay_label("Status Legend: MISSING=file absent, CORRUPT=failed to load, DIM ERR=invalid or "
                  "extreme dimensions (>4096), EXT WARN=unexpected extension.");
    if (overlay_button("Close"))
        g_val_panel.show = 0;
    overlay_end_panel();
#else
    (void) vis; /* suppress unused warning if compiled without overlay */
#endif
}

/* Hot-reload dependency poll callback defined after function to avoid cluttering main panel logic
 */
static int rogue_validation_panel_dep_changed(const char* path, void* user)
{
    (void) user;
    if (strncmp(g_val_panel.cast.path, path, sizeof g_val_panel.cast.path) == 0)
        g_val_panel.cast.load_attempted = 0;
    if (strncmp(g_val_panel.projectile.path, path, sizeof g_val_panel.projectile.path) == 0)
        g_val_panel.projectile.load_attempted = 0;
    if (strncmp(g_val_panel.impact.path, path, sizeof g_val_panel.impact.path) == 0)
        g_val_panel.impact.load_attempted = 0;
    if (strncmp(g_val_panel.aoe.path, path, sizeof g_val_panel.aoe.path) == 0)
        g_val_panel.aoe.load_attempted = 0;
    return 0;
}
