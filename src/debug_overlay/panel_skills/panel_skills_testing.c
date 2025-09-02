#include "panel_skills_testing.h"
#include "../../core/app/app_state.h"
#include "../../core/skills/skill_debug.h"
#include "../../graphics/effect_spec.h"
#include "../../graphics/sprite.h"
#include "../overlay_core.h"
#include "../overlay_theme.h"
#include "../widgets/overlay_widgets.h"
#include "panel_skills_shared.h"
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* Lightweight preview texture cache helpers (file-scope) */
typedef struct PreviewTex
{
    char path[256];
    RogueTexture tex;
    int ready;
} PreviewTex;

/* Small helper: map effect kind to human label (defensive for bounds) */
static const char* effect_kind_label(int kind)
{
    switch (kind)
    {
    case ROGUE_EFFECT_STAT_BUFF:
        return "STAT_BUFF";
    case ROGUE_EFFECT_DOT:
        return "DOT";
    case ROGUE_EFFECT_AURA:
        return "AURA";
    case ROGUE_EFFECT_HEAL:
        return "HEAL";
    case ROGUE_EFFECT_SPAWN_PROJECTILE:
        return "SPAWN_PROJECTILE";
    case ROGUE_EFFECT_DAMAGE:
        return "DAMAGE";
    case ROGUE_EFFECT_AOE_BLAST:
        return "AOE_BLAST";
    case ROGUE_EFFECT_TELEPORT:
        return "TELEPORT";
    default:
        return "UNKNOWN";
    }
}
static void preview_ensure_tex_local(PreviewTex* t, const char* path)
{
    if (!t)
        return;
    if (!path)
        path = "";
    if (strcmp(t->path, path) != 0)
    {
        if (t->ready)
        {
            rogue_texture_destroy(&t->tex);
            t->ready = 0;
        }
        t->path[0] = '\0';
        if (path[0])
        {
            if (rogue_texture_load(&t->tex, path))
            {
                strncpy(t->path, path, sizeof t->path - 1);
                t->path[sizeof t->path - 1] = '\0';
                t->ready = 1;
            }
        }
    }
}
void panel_skills_draw_testing(int sel)
{
    static float sim_duration_ms = 2000.0f;
    static float sim_tick_ms = 16.0f;
    static float sim_ap_regen_per_sec = 0.0f;
    static char prio_buf[128] = "";
    static char sim_result[256] = "";

    /* M4.2: Headless-safe textual live preview (timeline + effects + tick estimate) */
    {
        const char* headers[] = {"Property", "Value"};
        int sc = 0, sd = 0;
        if (overlay_table_begin("skills_live_preview", headers, 2, &sc, &sd, NULL))
        {
            /* Type/name */
            int stype = 0;
            (void) rogue_skill_debug_get_type(sel, &stype);
            static const char* type_names[10] = {"UNKNOWN", "MELEE",   "RANGED", "AOE_SPELL",
                                                 "BUFF",    "DEBUFF",  "HEAL",   "SUMMON",
                                                 "PASSIVE", "ULTIMATE"};
            const char* tname = (stype >= 0 && stype < 10) ? type_names[stype] : "UNKNOWN";
            const char* n = rogue_skill_debug_name(sel);
            if (!n)
                n = "<noname>";
            char line0[128];
            snprintf(line0, sizeof line0, "%s (%s)", n, tname);
            {
                const char* r0[] = {"Skill", line0};
                (void) overlay_table_row(r0, 2, 0, NULL);
            }

            /* Timing */
            float base_cd = 0.f, cd_red = 0.f, cast_ms = 0.f;
            (void) rogue_skill_debug_get_timing(sel, &base_cd, &cd_red, &cast_ms);
            char cast_buf[64];
            snprintf(cast_buf, sizeof cast_buf, "Cast: %.0f ms", cast_ms);
            const char* r1[] = {"Timeline", cast_buf};
            (void) overlay_table_row(r1, 2, 1, NULL);
            char cd_buf[64];
            snprintf(cd_buf, sizeof cd_buf, "Cooldown: %.0f ms (%.0f/rank)", base_cd, cd_red);
            const char* r2[] = {"Cooldown", cd_buf};
            (void) overlay_table_row(r2, 2, 2, NULL);

            /* Coeffs */
            RogueSkillCoeffParams cp;
            if (rogue_skill_debug_get_coeff(sel, &cp) == 0)
            {
                char cbuf[96];
                snprintf(cbuf, sizeof cbuf, "base=%.2f, per_rank=%.2f", cp.base_scalar,
                         cp.per_rank_scalar);
                const char* r3[] = {"Coeff", cbuf};
                (void) overlay_table_row(r3, 2, 3, NULL);
            }

            /* Effects summary and rough tick estimate */
            int primary_id = -1;
            struct RogueSkillEffectNode nodes[3];
            int node_count = 3;
            for (int i = 0; i < 3; ++i)
            {
                nodes[i].effect_spec_id = -1;
                nodes[i].delay_ms = 0.f;
                nodes[i].duration_ms = 0.f;
                nodes[i].repeat_count = 0;
                nodes[i].repeat_interval_ms = 0.f;
                nodes[i].require_player_health_below_pct = 0;
            }
            if (rogue_skill_debug_get_effects(sel, &primary_id, nodes, &node_count) != 0)
            {
                primary_id = -1;
                node_count = 0;
            }

            int tick_total = 0;
            /* Primary effect (from EffectSpec) */
            if (primary_id >= 0)
            {
                const RogueEffectSpec* es = rogue_effect_get(primary_id);
                if (es)
                {
                    char pbuf[128];
                    snprintf(pbuf, sizeof pbuf, "%d (%s) mag=%d", primary_id,
                             effect_kind_label(es->kind), es->magnitude);
                    const char* rp[] = {"Primary Effect", pbuf};
                    (void) overlay_table_row(rp, 2, 4, NULL);
                    int t = 1;
                    if (es->repeat_count > 0)
                        t += (int) es->repeat_count;
                    else if (es->pulse_period_ms > 0.f && es->duration_ms > 0.f)
                        t += (int) (es->duration_ms / es->pulse_period_ms);
                    if (t < 1)
                        t = 1;
                    tick_total += t;
                }
            }
            /* Additional nodes (per-skill timing) */
            for (int i = 0; i < node_count && i < 3; ++i)
            {
                int eid = nodes[i].effect_spec_id;
                const RogueEffectSpec* esn = (eid >= 0 ? rogue_effect_get(eid) : NULL);
                char nbuf[160];
                if (esn)
                {
                    snprintf(nbuf, sizeof nbuf,
                             "Node %d: %d (%s), delay=%.0f, reps=%d, int=%.0f, dur=%.0f", i + 1,
                             eid, effect_kind_label(esn->kind), nodes[i].delay_ms,
                             nodes[i].repeat_count, nodes[i].repeat_interval_ms,
                             nodes[i].duration_ms);
                }
                else
                {
                    snprintf(nbuf, sizeof nbuf, "Node %d: <none>", i + 1);
                }
                const char* rn[] = {"Effect Node", nbuf};
                (void) overlay_table_row(rn, 2, 5 + i, NULL);

                int t = 1;
                if (nodes[i].repeat_count > 0)
                    t += nodes[i].repeat_count;
                else if (nodes[i].duration_ms > 0.f && nodes[i].repeat_interval_ms > 0.f)
                    t += (int) (nodes[i].duration_ms / nodes[i].repeat_interval_ms);
                if (t < 1)
                    t = 1;
                tick_total += t;
            }
            char tb[64];
            snprintf(tb, sizeof tb, "%d", tick_total);
            {
                const char* rt[] = {"Est. Tick Count", tb};
                (void) overlay_table_row(rt, 2, 9, NULL);
            }
            overlay_table_end();
        }
    }

    overlay_label("Simulation Profile");
    overlay_slider_float("Duration (ms)", &sim_duration_ms, 50.0f, 60000.0f);
    overlay_slider_float("Tick (ms)", &sim_tick_ms, 1.0f, 100.0f);
    overlay_slider_float("AP regen (/sec)", &sim_ap_regen_per_sec, 0.0f, 200.0f);
    overlay_input_text("Priority IDs (comma)", prio_buf, sizeof prio_buf);

    if (overlay_button("Simulate"))
    {
        char profile[256];
        char prio_json[128] = {0};
        int pj = 0;
        prio_json[pj++] = '[';
        if (prio_buf[0] == '\0')
            pj += snprintf(prio_json + pj, (int) sizeof prio_json - pj, "%d", sel);
        else
        {
            for (const char* p = prio_buf; *p && pj + 1 < (int) sizeof prio_json; ++p)
            {
                char c = *p;
                if ((c >= '0' && c <= '9') || c == ',' || c == '-')
                    prio_json[pj++] = c;
            }
        }
        if (pj + 2 < (int) sizeof prio_json)
        {
            prio_json[pj++] = ']';
            prio_json[pj] = '\0';
        }
        else
        {
            prio_json[0] = '[';
            prio_json[1] = ']';
            prio_json[2] = '\0';
        }
        snprintf(profile, sizeof profile,
                 "{\"duration_ms\":%d,\"tick_ms\":%.1f,\"ap_regen_per_sec\":%.1f,\"priority\":%s}",
                 (int) sim_duration_ms, sim_tick_ms, sim_ap_regen_per_sec, prio_json);
        if (rogue_skill_debug_simulate(profile, sim_result, (int) sizeof sim_result) != 0)
            snprintf(sim_result, sizeof sim_result, "Simulation failed");
    }
    if (sim_result[0])
        overlay_label(sim_result);

    /* Real-time preview window (sprite-based) */
    static PreviewTex t_cast = {"", {0}};
    static PreviewTex t_proj = {"", {0}};
    static PreviewTex t_impact = {"", {0}};
    static PreviewTex t_aoe = {"", {0}};

    static int preview_enabled = 1;
    static int preview_autoplay = 1;
    static int preview_zoom = 2; /* 1..8 */
    overlay_checkbox("Enable Real-time Preview", &preview_enabled);
    overlay_checkbox("Auto-animate", &preview_autoplay);
    overlay_slider_int("Zoom", &preview_zoom, 1, 8);

    const int panel_x = 380, panel_y = 10; /* match orchestrator */
    const int pv_x = panel_x + 12, pv_y = panel_y + 260;
    const int pv_w = 396, pv_h = 140;
#ifdef ROGUE_HAVE_SDL
    if (!g_app.headless && g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect r = {pv_x, pv_y, pv_w, pv_h};
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r, th->panel_bg.g, th->panel_bg.b,
                               th->panel_bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                               th->panel_border.b, th->panel_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
    }
#endif

    if (preview_enabled)
    {
        int stype = 0;
        (void) rogue_skill_debug_get_type(sel, &stype);
        RogueSkillVisualParams vis;
        if (rogue_skill_debug_get_visuals(sel, &vis) == 0)
        {
            preview_ensure_tex_local(&t_cast, vis.cast_sprite_sheet);
            preview_ensure_tex_local(&t_proj, vis.projectile_sprite);
            preview_ensure_tex_local(&t_impact, vis.impact_sprite);
            preview_ensure_tex_local(&t_aoe, vis.aoe_sprite);

            static float anim_t = 0.0f;
            if (preview_autoplay)
                anim_t += overlay_last_dt();
            const PreviewTex* show = NULL;
            int is_sheet = 0;
            if (stype == 2 && t_proj.ready)
                show = &t_proj; /* RANGED */
            else if (stype == 3 && t_aoe.ready)
                show = &t_aoe; /* AOE */
            else if (t_cast.ready)
            {
                show = &t_cast;
                is_sheet = 1;
            }
            else if (t_impact.ready)
                show = &t_impact;

            if (show && show->ready)
            {
                RogueSprite spr = {0};
                spr.tex = (RogueTexture*) &show->tex;
                int cell_w = show->tex.w, cell_h = show->tex.h;
                int grid_w = vis.grid_width > 0 ? vis.grid_width : 1;
                int grid_h = vis.grid_height > 0 ? vis.grid_height : 1;
                int frames = vis.frame_count > 0 ? vis.frame_count : (grid_w * grid_h);
                if (is_sheet && grid_w > 0 && grid_h > 0)
                {
                    cell_w = (grid_w > 0 ? show->tex.w / grid_w : show->tex.w);
                    cell_h = (grid_h > 0 ? show->tex.h / grid_h : show->tex.h);
                    float fd = (vis.frame_duration_ms > 0 ? vis.frame_duration_ms : 100.0f);
                    int f = (int) (anim_t * 1000.0f / fd);
                    if (!vis.animation_loops && f >= frames)
                        f = frames - 1;
                    if (frames > 0)
                        f = f % frames;
                    int fx = (frames > 0 ? (f % grid_w) : 0);
                    int fy = (frames > 0 ? (f / grid_w) : 0);
                    spr.sx = fx * cell_w;
                    spr.sy = fy * cell_h;
                    spr.sw = cell_w;
                    spr.sh = cell_h;
                }
                else
                {
                    spr.sx = 0;
                    spr.sy = 0;
                    spr.sw = cell_w;
                    spr.sh = cell_h;
                }

                int scale = (preview_zoom < 1 ? 1 : preview_zoom);
                int dx = pv_x + (pv_w - spr.sw * scale) / 2;
                int dy = pv_y + (pv_h - spr.sh * scale) / 2;
                rogue_sprite_draw(&spr, dx, dy, scale);
            }
            else
            {
                overlay_label("Preview: (no sprite configured)");
            }
        }
    }
}
#endif
