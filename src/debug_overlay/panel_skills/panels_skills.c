#include "../../core/app/app_state.h"
#include "../../core/skills/skill_debug.h"
#include "../../core/skills/skills_coeffs.h"
#include "../../core/skills/skills_validate.h"
#include "../../game/buffs.h" /* for rogue_buffs_type_categories to derive palette categories */
#include "../../graphics/effect_spec.h"
#include "../../graphics/sprite.h"
#include "../overlay_core.h"
#include "../overlay_input.h"
#include "../widgets/overlay_widgets.h"
#include "panel_skills_audio.h"
#include "panel_skills_effects.h"
#include "panel_skills_overview.h"
#include "panel_skills_testing.h"
#include "panel_skills_visuals.h"
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

/* Small utility for box hit-testing (C-friendly) */
static int overlay_box_hit(int mx, int my, int bx, int by, int bw, int bh)
{
    return (mx >= bx && mx < bx + bw && my >= by && my < by + bh) ? 1 : 0;
}

/* Palette helper: map an EffectSpec to overlay categories based on kind/debuff/buff_type. */
static int palette_effect_categories(const RogueEffectSpec* es)
{
    if (!es)
        return 0;
    int cats = 0;
    if (es->debuff)
        cats |= ROGUE_BUFF_CAT_OFFENSIVE;
    if (es->kind == ROGUE_EFFECT_STAT_BUFF)
    {
        if (es->buff_type >= 0 && es->buff_type < ROGUE_BUFF_MAX)
            cats |= (int) rogue_buffs_type_categories((RogueBuffType) es->buff_type);
        else
            cats |= ROGUE_BUFF_CAT_UTILITY;
    }
    if (es->kind == ROGUE_EFFECT_AURA)
        cats |= ROGUE_BUFF_CAT_OFFENSIVE;
    return cats;
}

static void preview_ensure_tex(PreviewTex* t, const char* path)
{
    if (!t)
        return;
    if (!path)
        path = "";
    if (strcmp(t->path, path) != 0)
    {
        /* destroy old */
        if (t->ready)
        {
            rogue_texture_destroy(&t->tex);
            t->ready = 0;
        }
        t->path[0] = '\0';
        if (path[0])
        {
            /* try load */
            if (rogue_texture_load(&t->tex, path))
            {
                strncpy(t->path, path, sizeof t->path - 1);
                t->path[sizeof t->path - 1] = '\0';
                t->ready = 1;
            }
        }
    }
}

static void panel_skills(void* user)
{
    (void) user;
    // Use movable, persisted panel begin with a wider default to fit content.
    if (!overlay_begin_panel_auto("skills", "Skills", 360, 10, 480))
        return;
    /* Simple tabs (combo-based): Overview, Effects, Visuals, Audio, Testing */
    static int tab = 0;
    static const char* tab_names[] = {"Overview", "Effects", "Visuals", "Audio", "Testing"};
    overlay_combo("Tab", &tab, tab_names, 5);

    /* Persistent UI state */
    static float sim_duration_ms = 2000.0f;
    static float sim_tick_ms = 16.0f;
    static float sim_ap_regen_per_sec = 0.0f;
    static char prio_buf[128] = "";
    static char sim_result[256] = "";
    static int last_valid_ok = 1;
    static char last_valid_msg[128] = "OK";
    /* Sticky Validation Status banner (visible at top of panel) */
    {
        char vline[192];
        snprintf(vline, sizeof vline, "Validation: %s%s%s", last_valid_ok ? "OK" : "ERROR",
                 last_valid_ok ? "" : ": ", last_valid_ok ? "" : last_valid_msg);
        overlay_label(vline);
    }
    /* Template/new-skill helpers (Overview tab) */
    static int tmpl_id = -1;
    static char tmpl_filter[64] = "";
    static char new_name[64] = "";
    static int new_max_rank = 5;
    static float new_base_cd = 1000.f;
    static float new_cd_red = 0.f;
    static float new_cast_ms = 0.f;
    static int new_is_passive = 0;
    static int copy_coeffs = 1;

    const char* overrides_path = "build/skills_overrides.json";
    const char* base_skills_path = "assets/skills_uhf87f.json";
    static int auto_reload = 1;
    static int auto_reload_base = 0;

    int count = rogue_skill_debug_count();
    static int sel = 0;
    if (sel < 0)
        sel = 0;
    if (sel >= count)
        sel = count - 1;
    if (count <= 0)
    {
        overlay_label("No skills registered");
        overlay_end_panel();
        return;
    }
    overlay_slider_int("Skill Index", &sel, 0, count - 1);
    const char* name = rogue_skill_debug_name(sel);

    /* Overview -------------------------------------------------------------- */
    if (tab == 0)
    {
        panel_skills_draw_overview(&sel);
    }

    /* (Timing and coefficient editors are now handled inside the Overview tab) */

    /* Visuals tab -------------------------------------------------------------- */
    if (tab == 2)
    {
        panel_skills_draw_visuals(sel);
    }

    /* Audio tab --------------------------------------------------------------- */
    if (tab == 3)
    {
        panel_skills_draw_audio(sel);
    }

    /* Effects tab */
    if (tab == 1)
    {
        panel_skills_draw_effects(sel);
    }

    /* Testing tab: move simulation here and add a lightweight preview -------- */
    if (tab == 4)
    {
        panel_skills_draw_testing(sel);
    }

    if (overlay_button("Save Overrides JSON"))
    {
        char vmsg[192] = {0};
        int v = rogue_skill_debug_validate(vmsg, (int) sizeof vmsg);
        if (v != 0)
        {
            char warn[256];
            snprintf(warn, sizeof warn, "Validation failed: %s", vmsg[0] ? vmsg : "(no details)");
            overlay_label(warn);
        }
        int rc = (v == 0) ? rogue_skill_debug_save_overrides(overrides_path) : -3;
        char msg[128];
        snprintf(msg, sizeof msg, "Save: %s (%d)", (rc == 0 ? "OK" : "ERR"), rc);
        overlay_label(msg);
        /* Update sticky status based on cross-rule validation */
        char emsg[192] = {0};
        int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
        last_valid_ok = ok;
        if (!ok)
            snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
        else
            snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
    }
    if (overlay_button("Load Overrides JSON"))
    {
        int applied = rogue_skill_debug_load_overrides_file(overrides_path);
        char msg[128];
        snprintf(msg, sizeof msg, "Load: %s (%d)", (applied >= 0 ? "OK" : "ERR"), applied);
        overlay_label(msg);
        /* Update sticky status after load */
        char emsg[192] = {0};
        int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
        last_valid_ok = ok;
        if (!ok)
            snprintf(last_valid_msg, sizeof last_valid_msg, "%s", emsg[0] ? emsg : "error");
        else
            snprintf(last_valid_msg, sizeof last_valid_msg, "%s", "OK");
    }

    if (overlay_checkbox("Auto-Reload Overrides", &auto_reload))
    {
    }
    if (auto_reload)
    {
        int applied = rogue_skill_debug_autoreload_tick(overrides_path);
        if (applied > 0)
        {
            char msg[96];
            snprintf(msg, sizeof msg, "Auto-Reload applied: %d entries", applied);
            overlay_label(msg);
        }
    }
    if (overlay_checkbox("Auto-Reload Base Skills JSON", &auto_reload_base))
    {
    }
    if (auto_reload_base)
    {
        int loaded = rogue_skills_base_autoreload_tick(base_skills_path);
        if (loaded > 0)
        {
            char msg[96];
            snprintf(msg, sizeof msg, "Base reload: %d skills loaded", loaded);
            overlay_label(msg);
        }
    }

    overlay_end_panel();
}

void rogue_overlay_register_panel_skills(void)
{
    overlay_register_panel("skills", "Skills", panel_skills, NULL);
}

#endif
