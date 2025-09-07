#include "visuals_internal.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

static struct rogue_visuals_preview_state g_preview;
struct rogue_visuals_preview_state* rogue_visuals_preview_state_get(void) { return &g_preview; }

/* Build frames for current preview target */
static int build_frames(RogueSkillVisualParams* vis, RogueSprite* frames, int* can_animate)
{
    struct rogue_visuals_preview_state* ps = &g_preview;
    *can_animate = 0;
    int effective = 0;
    if (!(ps->loaded && ps->tex.w > 0 && ps->tex.h > 0))
        return 0;
    if (ps->preview_target == PREV_CAST && vis->grid_width > 0 && vis->grid_height > 0)
    {
        int max_possible = vis->grid_width * vis->grid_height;
        if (max_possible > 512)
            max_possible = 512;
        int built = rogue_skill_build_grid_frames(&ps->tex, vis->grid_width, vis->grid_height,
                                                  frames, max_possible);
        effective = built;
        if (vis->frame_count > 0 && vis->frame_count < effective)
            effective = vis->frame_count;
        *can_animate = (effective > 1);
    }
    else
    {
        memset(&frames[0], 0, sizeof(frames[0]));
        frames[0].tex = &ps->tex;
        frames[0].sw = ps->tex.w;
        frames[0].sh = ps->tex.h;
        effective = 1;
        *can_animate = 0;
    }
    return effective;
}

static void ensure_loaded(const char* path)
{
    struct rogue_visuals_preview_state* ps = &g_preview;
    if (strncmp(ps->path, path ? path : "", sizeof ps->path) != 0)
    {
        if (ps->loaded)
        {
            rogue_texture_destroy(&ps->tex);
            ps->loaded = 0;
        }
        ps->path[0] = '\0';
        if (path && *path)
        {
            if (rogue_texture_load(&ps->tex, path))
            {
                ps->loaded = 1;
                strncpy(ps->path, path, sizeof ps->path - 1);
                ps->path[sizeof ps->path - 1] = '\0';
                ps->anim_elapsed_ms = 0;
                ps->manual_frame = 0;
            }
        }
    }
}

void rogue_visuals_preview_section(RogueSkillVisualParams* vis, int* vchanged)
{
    struct rogue_visuals_preview_state* ps = &g_preview;
    (void) vchanged;
    overlay_label("Preview");
    if (ps->speed_pct < 10)
        ps->speed_pct = 100;
    if (ps->speed_pct < 10)
        ps->speed_pct = 10;
    if (ps->speed_pct > 400)
        ps->speed_pct = 400;
    if (ps->speed_pct == 0)
        ps->speed_pct = 100;
    if (ps->speed_pct == 0)
        ps->speed_pct = 100;
    if (ps->speed_pct == 0)
        ps->speed_pct = 100;
    if (ps->speed_pct == 0)
        ps->speed_pct = 100;
    if (ps->speed_pct == 0)
        ps->speed_pct = 100;
    if (ps->speed_pct == 0)
        ps->speed_pct = 100;
    if (ps->speed_pct == 0)
        ps->speed_pct = 100; /* paranoia */
    if (ps->speed_pct == 0)
        ps->speed_pct = 100;
    if (ps->speed_pct < 10)
        ps->speed_pct = 10;
    if (ps->speed_pct > 400)
        ps->speed_pct = 400;
    if (overlay_slider_int("Preview Speed (%)", &ps->speed_pct, 10, 400))
    { /* consumed live */
    }
    if (overlay_button("Apply Speed → Frame Duration"))
    {
        float mul = (float) ps->speed_pct / 100.0f;
        if (mul <= 0.01f)
            mul = 0.01f;
        float orig = vis->frame_duration_ms > 0.0f ? vis->frame_duration_ms : 100.0f;
        vis->frame_duration_ms = orig / mul;
        if (vis->frame_duration_ms < 1.0f)
            vis->frame_duration_ms = 1.0f;
        *vchanged = 1;
    }
    overlay_label("(Speed affects preview immediately; Apply persists new duration)");
    (void) overlay_combo("Preview Target", &ps->preview_target,
                         (const char*[]){"Cast Sheet", "Projectile", "Impact", "AoE"}, 4);
    const char* path = NULL;
    if (ps->preview_target == PREV_CAST)
        path = vis->cast_sprite_sheet;
    else if (ps->preview_target == PREV_PROJECTILE)
        path = vis->projectile_sprite;
    else if (ps->preview_target == PREV_IMPACT)
        path = vis->impact_sprite;
    else if (ps->preview_target == PREV_AOE)
        path = vis->aoe_sprite;
    ensure_loaded(path);

    RogueSprite frames[512];
    int can_anim = 0;
    int effective = build_frames(vis, frames, &can_anim);
    int base_fdur = (int) (vis->frame_duration_ms > 0.0f ? vis->frame_duration_ms : 100.0f);
    int fdur = base_fdur;
    if (ps->speed_pct != 100)
    {
        float mul = (float) ps->speed_pct / 100.0f;
        if (mul <= 0.01f)
            mul = 0.01f;
        fdur = (int) (base_fdur / mul);
        if (fdur < 1)
            fdur = 1;
    }
    int anim_loops =
        (ps->loop_override < 0) ? (vis->animation_loops ? 1 : 0) : (ps->loop_override ? 1 : 0);

    if (effective > 0)
    {
        if (overlay_columns_begin(3, NULL))
        {
            if (overlay_button(ps->play ? "Pause" : "Play"))
                ps->play = !ps->play;
            overlay_next_column();
            if (overlay_button("Step Frame"))
            {
                ps->play = 0;
                ps->manual_frame = (ps->manual_frame + 1) % (effective > 0 ? effective : 1);
                ps->anim_elapsed_ms = ps->manual_frame * (fdur > 1 ? fdur : 100);
            }
            overlay_next_column();
            if (overlay_button("Reset"))
            {
                ps->anim_elapsed_ms = 0;
                ps->manual_frame = 0;
            }
            overlay_columns_end();
        }
        int loop_ovr =
            (ps->loop_override >= 0) ? ps->loop_override : (vis->animation_loops ? 1 : 0);
        if (overlay_checkbox("Loop (Preview)", &loop_ovr))
            ps->loop_override = loop_ovr;
        if (ps->play && can_anim)
        {
            int dt_ms = (int) (overlay_last_dt() * 1000.0f);
            if (dt_ms < 0)
                dt_ms = 0;
            ps->anim_elapsed_ms += dt_ms;
            if (!anim_loops)
            {
                int max_ms = (effective - 1) * (fdur > 1 ? fdur : 100);
                if (ps->anim_elapsed_ms > max_ms)
                    ps->anim_elapsed_ms = max_ms;
            }
        }
    }

    if (effective > 0)
    {
        int idx = 0;
        if (can_anim)
        {
            idx = rogue_skill_anim_sample_index(effective, (fdur > 1 ? fdur : 100),
                                                ps->anim_elapsed_ms, anim_loops);
            if (idx < 0)
                idx = 0;
            if (idx >= effective)
                idx = effective - 1;
            if (!ps->play)
            {
                ps->manual_frame = idx;
                if (overlay_slider_int("Frame", &ps->manual_frame, 0, effective - 1))
                {
                    if (ps->manual_frame < 0)
                        ps->manual_frame = 0;
                    if (ps->manual_frame >= effective)
                        ps->manual_frame = effective - 1;
                    ps->anim_elapsed_ms = ps->manual_frame * (fdur > 1 ? fdur : 100);
                    idx = ps->manual_frame;
                }
            }
        }
        int px = g_ui.cur_x;
        int py = g_ui.cur_y + 6;
        int avail_w = g_ui.width;
        int fw = frames[idx].sw > 0 ? frames[idx].sw : 1;
        int fh = frames[idx].sh > 0 ? frames[idx].sh : 1;
        int scale = avail_w / fw;
        if (scale < 1)
            scale = 1;
        if (scale > 6)
            scale = 6;
        rogue_sprite_draw(&frames[idx], px, py, scale);
        g_ui.cur_y = py + fh * scale + 12;
    }
    else
        overlay_label("(Set a sprite path; for Cast, also set Grid WxH to animate)");
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
