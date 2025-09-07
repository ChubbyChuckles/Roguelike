#include "visuals_internal.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

void rogue_visuals_draw_core_fields(RogueSkillVisualParams* vis, int stype, int* vchanged)
{
    if (stype != 8)
    {
        *vchanged |= overlay_input_text("Cast Sprite Sheet", vis->cast_sprite_sheet,
                                        (int) sizeof vis->cast_sprite_sheet);
        *vchanged |= overlay_slider_int("Frame Count", &vis->frame_count, 0, 512);
        *vchanged |=
            overlay_slider_float("Frame Duration (ms)", &vis->frame_duration_ms, 0.0f, 2000.0f);
        *vchanged |= overlay_checkbox("Animation Loops", &vis->animation_loops);
        *vchanged |= overlay_slider_int("Grid Width", &vis->grid_width, 0, 128);
        *vchanged |= overlay_slider_int("Grid Height", &vis->grid_height, 0, 128);
    }
    *vchanged |=
        overlay_input_text("Impact Sprite", vis->impact_sprite, (int) sizeof vis->impact_sprite);
    if (stype == 3)
    { /* AOE */
        *vchanged |=
            overlay_input_text("AoE Sprite", vis->aoe_sprite, (int) sizeof vis->aoe_sprite);
        const char* aoe_items[] = {"None", "Circle", "Cone", "Line"};
        *vchanged |= overlay_combo("AoE Shape", &vis->aoe_shape, aoe_items, 4);
        *vchanged |= overlay_slider_float("AoE Radius", &vis->aoe_radius, 0.0f, 1000.0f);
        *vchanged |= overlay_slider_float("AoE Angle", &vis->aoe_angle, 0.0f, 360.0f);
    }
    if (stype == 2)
    { /* RANGED */
        *vchanged |= overlay_input_text("Projectile Sprite", vis->projectile_sprite,
                                        (int) sizeof vis->projectile_sprite);
        *vchanged |=
            overlay_slider_float("Projectile Velocity", &vis->projectile_velocity, 0.0f, 5000.0f);
        const char* traj_items[] = {"Linear", "Arc", "Homing", "Scatter"};
        *vchanged |= overlay_combo("Trajectory Type", &vis->trajectory_type, traj_items, 4);
        *vchanged |= overlay_slider_int("Pierce Count", &vis->pierce_count, 0, 50);
        *vchanged |= overlay_slider_float("Homing Strength", &vis->homing_strength, 0.0f, 100.0f);
    }
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
