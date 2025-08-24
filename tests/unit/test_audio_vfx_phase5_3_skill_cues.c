/* Prevent SDL from redefining main on Windows test binaries */
#define SDL_MAIN_HANDLED 1
#include "../../src/audio_vfx/effects.h"
#include "../../src/core/app/app_state.h"
#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int cb_noop(const RogueSkillDef* def, struct RogueSkillState* st,
                   const struct RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return 1;
}

int main(void)
{
    /* Minimal event bus needed by skills systems */
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("audio_vfx_test_bus");
    assert(rogue_event_bus_init(&cfg) && "event bus init");
    /* Init minimal skills */
    rogue_skills_init();
    /* Allow ranking up */
    g_app.talent_points = 2;
    g_app.player.level = 10;

    /* Define a simple casted skill */
    RogueSkillDef s;
    memset(&s, 0, sizeof s);
    s.name = "CueTest";
    s.max_rank = 1;
    s.base_cooldown_ms = 0;
    s.on_activate = cb_noop;
    s.cast_type = 1;        /* cast */
    s.cast_time_ms = 64.0f; /* quick */
    int sid = rogue_skill_register(&s);
    assert(sid >= 0);
    int rrank = rogue_skill_rank_up(sid);
    if (rrank != 1)
    {
        fprintf(stderr, "rank_up failed: sid=%d talent=%d level=%d ret=%d\n", sid,
                g_app.talent_points, g_app.player.level, rrank);
        return 3;
    }

    /* Clean state and VFX def for cues */
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_fx_map_clear();
    rogue_vfx_registry_register("skill_fx", ROGUE_VFX_LAYER_UI, 100, 0);
    rogue_vfx_registry_set_emitter("skill_fx", 60.0f, 50, 16);

    /* Map start and end */
    char key_start[48];
    snprintf(key_start, sizeof key_start, "skill/%d/start", sid);
    char key_end[48];
    snprintf(key_end, sizeof key_end, "skill/%d/end", sid);
    rogue_fx_map_register(key_start, ROGUE_FX_MAP_VFX, "skill_fx", ROGUE_FX_PRI_UI);
    rogue_fx_map_register(key_end, ROGUE_FX_MAP_VFX, "skill_fx", ROGUE_FX_PRI_UI);

    /* Clear active and begin frame */
    rogue_vfx_clear_active();
    rogue_fx_frame_begin(1);

    /* Activate -> should trigger start cue immediately */
    RogueSkillCtx ctx = {0};
    int r = rogue_skill_try_activate(sid, &ctx);
    assert(r == 1);

    rogue_fx_frame_end();
    int processed1 = rogue_fx_dispatch_process();
    /* Use smaller steps to avoid expiring exactly at lifetime boundary */
    for (int i = 0; i < 4; ++i)
        rogue_vfx_update(16);

    int vfx1 = rogue_vfx_active_count();
    int parts1 = rogue_vfx_particles_active_count();

    /* Advance skill to finish cast, expect end cue.
       Run updates inside FX frames so triggers are enqueued during a valid frame. */
    int processed2 = 0;
    for (int i = 0; i < 6; ++i)
    {
        rogue_fx_frame_begin(2 + i);
        rogue_skills_update(16.0);
        rogue_fx_frame_end();
        processed2 += rogue_fx_dispatch_process();
        rogue_vfx_update(16);
    }

    int vfx2 = rogue_vfx_active_count();
    int parts2 = rogue_vfx_particles_active_count();

    /* Allow short-lived instances to expire as long as particles exist */
    int ok1 = (vfx1 > 0) || (parts1 > 0);
    int ok2 = (vfx2 > 0) || (parts2 > 0);
    /* Consider success if both events were processed and we observed visible effects in either
     * phase */
    int ok_any = ok1 || ok2;
    if (processed1 <= 0 || processed2 <= 0 || !ok_any)
    {
        fprintf(stderr, "p1=%d p2=%d v1=%d v2=%d pr1=%d pr2=%d\n", processed1, processed2, vfx1,
                vfx2, parts1, parts2);
        return 2;
    }

    rogue_skills_shutdown();
    rogue_event_bus_shutdown();
    puts("test_audio_vfx_phase5_3_skill_cues OK");
    return 0;
}
