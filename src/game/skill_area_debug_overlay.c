#include "skill_area_debug_overlay.h"
#include "../core/app/app_state.h"
#include "../core/skills/skills.h"
#include "../graphics/effect_spec.h"
#include <math.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

static void draw_circle_gradient(int cx, int cy, int radius_px, unsigned char r, unsigned char g,
                                 unsigned char b, unsigned char a)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    if (radius_px <= 0)
        return;
    /* Simple radial alpha falloff: sample along spokes for performance */
    int spokes = 64; /* coarse ring sufficient for debug */
    for (int i = 0; i < spokes; ++i)
    {
        float t = (float) i / (float) spokes;
        float ang = t * 6.28318530718f;
        float dx = cosf(ang);
        float dy = sinf(ang);
        for (int s = 0; s <= radius_px; s += 2)
        {
            float frac = (float) s / (float) radius_px; /* 0 center -> 1 edge */
            unsigned char aa = (unsigned char) (a * (1.0f - frac));
            SDL_SetRenderDrawColor(g_app.renderer, r, g, b, aa);
            SDL_RenderDrawPoint(g_app.renderer, cx + (int) (dx * s), cy + (int) (dy * s));
        }
    }
#else
    (void) cx;
    (void) cy;
    (void) radius_px;
    (void) r;
    (void) g;
    (void) b;
    (void) a;
#endif
}

void rogue_skill_area_debug_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer || !g_app.show_skill_area_overlay)
        return;
    int tsz = g_app.tile_size ? g_app.tile_size : 32;
    /* Player center in pixels */
    int px = (int) (g_app.player.base.pos.x * tsz - g_app.cam_x);
    int py = (int) (g_app.player.base.pos.y * tsz - g_app.cam_y);
    /* Render all active AURAs as blue gradient rings */
    int ac = rogue_effect_active_aura_count();
    for (int i = 0; i < ac; ++i)
    {
        int eff_id = -1;
        double end_ms = 0.0;
        if (!rogue_effect_active_aura_get(i, &eff_id, &end_ms))
            continue;
        const RogueEffectSpec* s = rogue_effect_get(eff_id);
        if (!s || s->kind != ROGUE_EFFECT_AURA)
            continue;
        int rad_px = (int) (s->aura_radius * (float) tsz);
        draw_circle_gradient(px, py, rad_px, 80, 160, 255, 160);
    }
    /* Channel area: if current skill def references an AURA and is channeling, tint green */
    for (int sid = 0; sid < g_app.skill_count; ++sid)
    {
        const struct RogueSkillState* st = &g_app.skill_states[sid];
        if (!st->channel_active)
            continue;
        const struct RogueSkillDef* def = &g_app.skill_defs[sid];
        if (def->effect_spec_id >= 0)
        {
            const RogueEffectSpec* s = rogue_effect_get(def->effect_spec_id);
            if (s && s->kind == ROGUE_EFFECT_AURA && s->aura_radius > 0.0f)
            {
                int rad_px = (int) (s->aura_radius * (float) tsz);
                draw_circle_gradient(px, py, rad_px, 120, 255, 120, 160);
            }
        }
    }
#endif
}
