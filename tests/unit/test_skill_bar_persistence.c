#include "core/skills.h"
#include "core/persistence.h"
#include "core/app_state.h"
#include <stdio.h>

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

int main(void){
    rogue_persistence_set_paths("test_skillbar_stats.tmp","test_skillbar_gen.tmp");
    rogue_skills_init();
    RogueSkillDef a={-1,"Alpha","icon_a",2, 1000,100,NULL};
    RogueSkillDef b={-1,"Beta","icon_b",2, 1000,100,NULL};
    int A=rogue_skill_register(&a); int B=rogue_skill_register(&b);
    if(A!=0||B!=1){ printf("bad ids\n"); return 1; }
    g_app.talent_points=5; rogue_skill_rank_up(A); rogue_skill_rank_up(B);
    g_app.skill_bar[0]=A; g_app.skill_bar[1]=B; g_app.skill_bar[2]=-1;
    rogue_persistence_save_player_stats();
    /* mutate */
    g_app.skill_bar[0]=-1; g_app.skill_bar[1]=-1;
    g_app.skill_states[0].rank=0; g_app.skill_states[1].rank=0; g_app.talent_points=0;
    /* fresh session */
    rogue_skills_shutdown(); rogue_skills_init();
    A=rogue_skill_register(&a); B=rogue_skill_register(&b);
    rogue_persistence_load_player_stats();
    if(g_app.skill_bar[0]!=0 || g_app.skill_bar[1]!=1){ printf("bar mismatch %d %d\n", g_app.skill_bar[0], g_app.skill_bar[1]); return 2; }
    const RogueSkillState* sa=rogue_skill_get_state(0); const RogueSkillState* sb=rogue_skill_get_state(1);
    if(!sa||!sb||sa->rank!=1||sb->rank!=1){ printf("rank mismatch\n"); return 3; }
    return 0;
}
