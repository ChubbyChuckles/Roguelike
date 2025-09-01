#include "panel_skills_visuals.h"
#include "../../core/skills/skill_debug.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "panel_skills_shared.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY
void panel_skills_draw_visuals(int sel)
{
    overlay_label("Visuals");
    static RogueSkillVisualParams vis;
    if (rogue_skill_debug_get_visuals(sel, &vis) == 0)
    {
        int stype = 0;
        (void) rogue_skill_debug_get_type(sel, &stype);
        int vchanged = 0;
        /* Core animation/sprite-sheet (non-passive) */
        if (stype != 8 /* PASSIVE */)
        {
            vchanged |= overlay_input_text("Cast Sprite Sheet", vis.cast_sprite_sheet,
                                           (int) sizeof vis.cast_sprite_sheet);
            vchanged |= overlay_slider_int("Frame Count", &vis.frame_count, 0, 512);
            vchanged |=
                overlay_slider_float("Frame Duration (ms)", &vis.frame_duration_ms, 0.0f, 2000.0f);
            vchanged |= overlay_checkbox("Animation Loops", &vis.animation_loops);
            vchanged |= overlay_slider_int("Grid Width", &vis.grid_width, 0, 128);
            vchanged |= overlay_slider_int("Grid Height", &vis.grid_height, 0, 128);
        }
        /* Impact sprite is common */
        vchanged |=
            overlay_input_text("Impact Sprite", vis.impact_sprite, (int) sizeof vis.impact_sprite);
        /* AoE fields for AoE spells */
        if (stype == 3 /* AOE_SPELL */)
        {
            vchanged |=
                overlay_input_text("AoE Sprite", vis.aoe_sprite, (int) sizeof vis.aoe_sprite);
            vchanged |= overlay_slider_int("AoE Shape (0 none,1 circle,2 cone,3 line)",
                                           &vis.aoe_shape, 0, 3);
            vchanged |= overlay_slider_float("AoE Radius", &vis.aoe_radius, 0.0f, 1000.0f);
            vchanged |= overlay_slider_float("AoE Angle", &vis.aoe_angle, 0.0f, 360.0f);
        }
        /* Projectile fields for ranged */
        if (stype == 2 /* RANGED */)
        {
            vchanged |= overlay_input_text("Projectile Sprite", vis.projectile_sprite,
                                           (int) sizeof vis.projectile_sprite);
            vchanged |= overlay_slider_float("Projectile Velocity", &vis.projectile_velocity, 0.0f,
                                             5000.0f);
            vchanged |= overlay_slider_int("Trajectory Type (0 lin,1 arc,2 homing,3 scatter)",
                                           &vis.trajectory_type, 0, 3);
            vchanged |= overlay_slider_int("Pierce Count", &vis.pierce_count, 0, 50);
            vchanged |= overlay_slider_float("Homing Strength", &vis.homing_strength, 0.0f, 100.0f);
        }
        if (vchanged)
        {
            (void) rogue_skill_debug_set_visuals(sel, &vis);
            panel_skills_save_overrides_and_refresh();
        }
    }
}
#endif
