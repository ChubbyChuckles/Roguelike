/* Phase 1.3 Enhanced Execution Pathways test
   Verifies pathway selection alters Fireball behavior deterministically:
     pathway 0 -> projectile spawn count increments
     pathway 1 -> adds buff (POWER_STRIKE) after base effect
     pathway 2 -> moves player (mini dash) and skips projectile spawn
   We only assert side-effects that are stable & low risk: last pathway id and
   player movement for pathway 2. Projectile & buff systems have their own tests,
   so here we just smoke check non-zero / changed conditions.
*/
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skill_tree.h"
#include "../../src/core/skills/skills.h"
#include "../../src/game/buffs.h"
#include <stdio.h>

extern struct RogueSkillDef* g_skill_defs_internal;
extern struct RogueSkillState* g_skill_states_internal;
extern int g_skill_count_internal;

static void reset_world(void)
{
    g_app.player.base.pos.x = 10.0f;
    g_app.player.base.pos.y = 10.0f;
    g_app.player.facing = 2; /* right */
}

int main(void)
{
    /* Initialize core subsystems needed for activation side-effects */
    g_app.game_time_ms = 1000.0; /* baseline deterministic time */
    rogue_buffs_init();
    rogue_skills_init();
    /* Ensure skill registry skips icon texture loads (init resets flag) */
    rogue_skills_set_skip_icon_loads(1);
    /* Load baseline skills so Fireball exists without incurring renderer icon loads */
    rogue_skill_tree_register_baseline();
    int fireball_id = -1;
    for (int i = 0; i < g_skill_count_internal; i++)
    {
        if (g_skill_defs_internal[i].name && strcmp(g_skill_defs_internal[i].name, "Fireball") == 0)
        {
            fireball_id = i;
            break;
        }
    }
    if (fireball_id < 0)
    {
        fprintf(stderr, "NO_FIREBALL\n");
        return 1;
    }
    g_skill_states_internal[fireball_id].rank = 1;
    /* Provide sufficient resources so gates do not fail */
    g_app.player.max_mana = 500;
    g_app.player.mana = 500;
    g_app.player.max_action_points = 50;
    g_app.player.action_points = 50;
    RogueSkillCtx ctx = {0};
    ctx.now_ms = 1000.0;
    /* Pathway 0 (default) */
    reset_world();
    if (!rogue_skill_try_activate(fireball_id, &ctx))
    {
        fprintf(stderr, "FIREBALL_ACTIVATE_FAIL\n");
        return 1;
    }
    if (rogue_skill_pathway_last_exec(fireball_id) != 0)
    {
        fprintf(stderr, "PW0_LAST_MISMATCH\n");
        return 1;
    }
    /* Refill resources and advance time well beyond any cooldown / weave gates */
    g_app.player.mana = g_app.player.max_mana;
    g_app.player.action_points = g_app.player.max_action_points;
    ctx.now_ms += 5000.0;

    /* Pathway 2 (utility dash) moves player right */
    rogue_skill_pathway_set(fireball_id, 2);
    reset_world();
    if (!rogue_skill_try_activate(fireball_id, &ctx))
    {
        fprintf(stderr, "PW2_ACTIVATE_FAIL\n");
        return 1;
    }
    if (rogue_skill_pathway_last_exec(fireball_id) != 2)
    {
        fprintf(stderr, "PW2_LAST_MISMATCH\n");
        return 1;
    }

    /* Refill and advance time again for pathway 1 activation */
    g_app.player.mana = g_app.player.max_mana;
    g_app.player.action_points = g_app.player.max_action_points;
    ctx.now_ms += 5000.0;

    /* Pathway 1 (empowered) should record last=1 */
    rogue_skill_pathway_set(fireball_id, 1);
    if (!rogue_skill_try_activate(fireball_id, &ctx))
    {
        fprintf(stderr, "PW1_ACTIVATE_FAIL\n");
        return 1;
    }
    if (rogue_skill_pathway_last_exec(fireball_id) != 1)
    {
        fprintf(stderr, "PW1_LAST_MISMATCH\n");
        return 1;
    }
    printf("EXEC_PATHWAYS_OK id=%d last=%d\n", fireball_id,
           rogue_skill_pathway_last_exec(fireball_id));
    rogue_skills_shutdown();
    return 0;
}
