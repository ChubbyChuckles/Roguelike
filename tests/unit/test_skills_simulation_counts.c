#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Simple effect: always consume so instant skills register as activations */
static int always_consume(const RogueSkillDef* def, struct RogueSkillState* st,
                          const RogueSkillCtx* ctx)
{
    (void) def;
    (void) st;
    (void) ctx;
    return ROGUE_ACT_CONSUMED;
}

static int extract_int(const char* s, const char* key)
{
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\":", key);
    const char* p = strstr(s, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t')
        ++p;
    return (int) strtol(p, NULL, 10);
}

static int extract_count_for_id(const char* s, int id)
{
    char pat[64];
    snprintf(pat, sizeof pat, "\"id\":%d,\"count\":", id);
    const char* p = strstr(s, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    return (int) strtol(p, NULL, 10);
}

int main(void)
{
    rogue_skills_init();

    /* Define two simple active skills with no AP cost and predictable cooldowns */
    RogueSkillDef s0 = {0};
    s0.name = "S0";
    s0.max_rank = 1;
    s0.base_cooldown_ms = 1000.0f;
    s0.action_point_cost = 0.0f;
    s0.on_activate = always_consume;
    s0.effect_spec_id = -1;
    RogueSkillDef s1 = {0};
    s1.name = "S1";
    s1.max_rank = 1;
    s1.base_cooldown_ms = 800.0f;
    s1.action_point_cost = 0.0f;
    s1.on_activate = always_consume;
    s1.effect_spec_id = -1;

    int id0 = rogue_skill_register(&s0);
    int id1 = rogue_skill_register(&s1);
    assert(id0 == 0 && id1 == 1);

    /* Unlock skills (rank 1) so they can be activated by the simulator */
    g_app.talent_points = 2;
    int r0 = rogue_skill_rank_up(id0);
    int r1 = rogue_skill_rank_up(id1);
    assert(r0 == 1 && r1 == 1);

    char out[256];
    int rc = rogue_skill_debug_simulate("{\"duration_ms\":2000,\"priority\":[0,1]}", out,
                                        (int) sizeof out);
    assert(rc == 0);
    /* Note: With 16ms ticks and one activation per tick, cooldown boundaries snap to the
       next tick. In this setup the engine produces 4 total casts within 2000ms:
       S0=2, S1=2. This assertion locks in the deterministic behavior. */
    /* Diagnostic: print JSON result for visibility during failures */
    printf("sim: %s\n", out);
    int total = extract_int(out, "total_casts");
    int c0 = extract_count_for_id(out, 0);
    int c1 = extract_count_for_id(out, 1);
    assert(total == 4);
    assert(c0 == 2);
    assert(c1 == 2);

    rogue_skills_shutdown();
    printf("OK skills_simulation_counts\n");
    return 0;
}
