#include "../../../core/app/app_state.h" /* g_app */
#include "visuals_internal.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

void rogue_visuals_dependency_viewer(RogueSkillVisualParams* vis)
{
    static int show = 0;
    if (overlay_button("Open Asset Dependency Viewer"))
        show = 1;
    if (!show)
        return;
    overlay_begin_panel("Asset Dependency Viewer", 1060, 60, 340);
    overlay_label("Sprites & Sounds referenced by selected skill (live)");
    const char* sprite_paths[4] = {vis->cast_sprite_sheet, vis->projectile_sprite,
                                   vis->impact_sprite, vis->aoe_sprite};
    const char* sprite_labels[4] = {"Cast", "Projectile", "Impact", "AoE"};
    for (int i = 0; i < 4; ++i)
    {
        const char* sp = sprite_paths[i];
        if (!sp || !*sp)
        {
            char line[128];
            snprintf(line, sizeof line, "%s: (unset)", sprite_labels[i]);
            overlay_label(line);
            continue;
        }
        int missing = 0;
        FILE* f = fopen(sp, "rb");
        if (f)
            fclose(f);
        else
            missing = 1;
        char line[256];
        snprintf(line, sizeof line, "%s: %s %s", sprite_labels[i], sp,
                 missing ? "(MISSING)" : "(OK)");
        overlay_label(line);
    }
    overlay_label("(Audio id listing placeholder – integrate when debug getter available)");
    overlay_label("Validation: Missing paths flagged. Thumbnail below when renderer present.");
#ifdef ROGUE_HAVE_SDL
    if (g_app.skill_icon_textures)
    { /* lightweight presence check */
    }
    if (g_app.renderer)
    {
        if (vis->cast_sprite_sheet[0])
        {
            static RogueTexture thumb;
            static char thumb_path[256] = {0};
            if (strncmp(thumb_path, vis->cast_sprite_sheet, sizeof thumb_path) != 0)
            {
                if (thumb.handle)
                    rogue_texture_destroy(&thumb);
                if (rogue_texture_load(&thumb, vis->cast_sprite_sheet))
                {
                    strncpy(thumb_path, vis->cast_sprite_sheet, sizeof thumb_path - 1);
                    thumb_path[sizeof thumb_path - 1] = '\0';
                }
            }
            if (thumb.handle && thumb.w > 0 && thumb.h > 0)
            {
                RogueSprite spr = {0};
                spr.tex = &thumb;
                spr.sw = thumb.w;
                spr.sh = thumb.h;
                int thumb_w = 64;
                int scale = spr.sw > 0 ? thumb_w / spr.sw : 1;
                if (scale < 1)
                    scale = 1;
                if (scale > 4)
                    scale = 4;
                int px = g_ui.cur_x;
                int py = g_ui.cur_y + 4;
                rogue_sprite_draw(&spr, px, py, scale);
                g_ui.cur_y = py + spr.sh * scale + 4;
            }
        }
    }
#endif
    if (overlay_button("Close Viewer"))
        show = 0;
    overlay_end_panel();
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
