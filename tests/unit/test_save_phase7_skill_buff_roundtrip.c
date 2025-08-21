#include "core/save_manager.h"
#include "core/skills.h"
#include "../../src/core/buffs.h"
#include "core/app_state.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Minimal harness: initialize skills & buffs, simulate state, save, mutate, load, verify restoration */

static int g_fail=0;
#define CHECK(cond,msg) do{ if(!(cond)){ printf("FAIL:%s line %d: %s\n", __FILE__, __LINE__, msg); g_fail=1; } }while(0)

int main(void){
    setvbuf(stdout,NULL,_IONBF,0);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_skills_init();
    rogue_buffs_init();

    /* Register core components (player/world/inventory/skills/buffs/vendor/strings) */
    extern void rogue_register_core_save_components(void); rogue_register_core_save_components();

    /* Ensure at least a few skills exist; if registry empty we fabricate three simple defs inline via public API assumptions */
    /* (If skills already loaded elsewhere, we just rely on existing count.) */
    if(rogue_skill_get_def(0)==NULL){
        RogueSkillDef def = {0};
        def.max_rank=5; def.base_cooldown_ms=1000; def.name="S0"; def.id=rogue_skill_register(&def);
        def.id=1; def.name="S1"; rogue_skill_register(&def);
        def.id=2; def.name="S2"; rogue_skill_register(&def);
    }
    int scount=0; while(rogue_skill_get_def(scount)) scount++;
    if(scount<3){ printf("FAIL: need >=3 skills\n"); return 1; }

    /* Mutate some skill state */
    struct RogueSkillState* s0=(struct RogueSkillState*)rogue_skill_get_state(0);
    struct RogueSkillState* s1=(struct RogueSkillState*)rogue_skill_get_state(1);
    s0->rank=3; s0->cooldown_end_ms=12345.0; s0->cast_progress_ms=150.0; s0->channel_end_ms=0.0; s0->next_charge_ready_ms=2222.0; s0->charges_cur=2; s0->casting_active=1; s0->channel_active=0;
    s1->rank=1; s1->cooldown_end_ms=888.0; s1->cast_progress_ms=0.0; s1->channel_end_ms=5555.0; s1->next_charge_ready_ms=0.0; s1->charges_cur=0; s1->casting_active=0; s1->channel_active=1;

    /* Apply two buffs */
    double now=10000.0; g_app.game_time_ms=now; /* set global time */
    rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE, 4, 5000.0, now, ROGUE_BUFF_STACK_ADD, 0); /* ends at 15000 */
    rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH, 2, 3000.0, now, ROGUE_BUFF_STACK_ADD, 0); /* ends at 13000 */

    if(rogue_save_manager_save_slot(0)!=0){ printf("FAIL: initial save\n"); return 1; }

    /* Mutate in-memory state to ensure load restores values */
    s0->rank=0; s0->cooldown_end_ms=0; s0->cast_progress_ms=0; s0->channel_end_ms=0; s0->next_charge_ready_ms=0; s0->charges_cur=0; s0->casting_active=0; s0->channel_active=0;
    s1->rank=0; s1->cooldown_end_ms=0; s1->cast_progress_ms=0; s1->channel_end_ms=0; s1->next_charge_ready_ms=0; s1->charges_cur=0; s1->casting_active=0; s1->channel_active=0;
    /* Advance time so remaining durations change if we had stored absolutes; using relative we expect restoration roughly preserved independent of current now. */
    g_app.game_time_ms = now + 1000.0; /* 1s later */
    /* Clear buffs by reinit to guarantee load repopulates */
    rogue_buffs_init();

    if(rogue_save_manager_load_slot(0)!=0){ printf("FAIL: load\n"); return 1; }

    /* Validate skills restored */
    CHECK(s0->rank==3, "skill0 rank");
    CHECK(s0->cooldown_end_ms==12345.0, "skill0 cd");
    CHECK(s0->cast_progress_ms==150.0, "skill0 cast_progress");
    CHECK(s0->next_charge_ready_ms==2222.0, "skill0 next_charge");
    CHECK(s0->charges_cur==2, "skill0 charges_cur");
    CHECK(s0->casting_active==1, "skill0 casting_active");
    CHECK(s1->channel_end_ms==5555.0, "skill1 channel_end");
    CHECK(s1->channel_active==1, "skill1 channel_active");

    /* Validate buffs roundtrip: we expect two active buffs with magnitudes >= original (stacking may merge types) */
    int total_power = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
    int total_str = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    CHECK(total_power>=4, "power buff magnitude");
    CHECK(total_str>=2, "strength buff magnitude");

    if(g_fail){ printf("FAILURES\n"); return 1; }
    printf("OK:save_phase7_skill_buff_roundtrip\n");
    return 0;
}
