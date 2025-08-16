#include "core/skills.h"
#include "core/persistence.h"
#include "core/app_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RogueAppState g_app; /* zero init */
RoguePlayer g_exposed_player_for_stats; /* unused */

/* Minimal stubs referenced during load (player recalculation) */
void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

int main(void){
    /* Use temp file names so we don't clobber any real data */
    rogue_persistence_set_paths("test_player_stats.tmp", "test_gen_params.tmp");
    /* Init skills and register baseline sample skills */
    rogue_skills_init();
    /* Register 2 deterministic skills for test */
    RogueSkillDef a={0}; a.id=-1; a.name="Alpha"; a.icon="icon_a"; a.max_rank=5; a.skill_strength=0; a.base_cooldown_ms=2000.0f; a.cooldown_reduction_ms_per_rank=200.0f;
    RogueSkillDef b={0}; b.id=-1; b.name="Beta"; b.icon="icon_b"; b.max_rank=3; b.skill_strength=0; b.base_cooldown_ms=3000.0f; b.cooldown_reduction_ms_per_rank=300.0f;
    int idA = rogue_skill_register(&a);
    int idB = rogue_skill_register(&b);
    if(idA!=0 || idB!=1){ printf("bad ids %d %d\n", idA, idB); return 1; }
    g_app.talent_points = 10; /* seed pool */
    if(rogue_skill_rank_up(idA)!=1) return 2;
    if(rogue_skill_rank_up(idA)!=2) return 3;
    if(rogue_skill_rank_up(idB)!=1) return 4;
    /* Save stats (writes ranks + talent points) */
    rogue_persistence_save_player_stats();
    int saved_tp = g_app.talent_points;
    /* Mutate state to confirm reload overwrites */
    g_app.talent_points = 0;
    g_app.skill_states[0].rank = 0;
    g_app.skill_states[1].rank = 0;
    /* Re-init skills to simulate fresh process, then register in SAME order so ids match */
    rogue_skills_shutdown();
    rogue_skills_init();
    idA = rogue_skill_register(&a);
    idB = rogue_skill_register(&b);
    if(idA!=0 || idB!=1){ printf("bad ids2 %d %d\n", idA, idB); return 5; }
    /* Load player stats which should restore ranks + talent points */
    rogue_persistence_load_player_stats();
    if(g_app.talent_points != saved_tp){ printf("tp mismatch %d vs %d\n", g_app.talent_points, saved_tp); return 6; }
    const RogueSkillState* sa = rogue_skill_get_state(0);
    const RogueSkillState* sb = rogue_skill_get_state(1);
    if(!sa || !sb){ printf("missing states\n"); return 7; }
    if(sa->rank != 2 || sb->rank != 1){ printf("rank mismatch %d %d\n", sa->rank, sb->rank); return 8; }
    /* Clean */
    rogue_skills_shutdown();
    /* Done */
    return 0;
}
