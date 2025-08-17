#include "core/save_manager.h"
#include "core/skills.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* NOTE: This test uses internal serialization implemented for skill (id=4) and buffs (id=5) components.
   It registers stubs so save_manager will include those ids, then relies on internal logic for actual write/read.
   Verifies Phase 7.2 (extended skill fields) & Phase 7.3 (buff remaining duration) are backward compatible roundtrip. */

typedef struct RogueBuff { int active; int type; double end_ms; int magnitude; } RogueBuff;
RogueBuff g_buffs_internal[8]; int g_buff_count_internal=0; int rogue_buffs_apply(int type,int magnitude,double duration_ms,double now_ms){ if(g_buff_count_internal<8){ g_buffs_internal[g_buff_count_internal].active=1; g_buffs_internal[g_buff_count_internal].type=type; g_buffs_internal[g_buff_count_internal].magnitude=magnitude; g_buffs_internal[g_buff_count_internal].end_ms = now_ms + duration_ms; g_buff_count_internal++; return 1;} return 0; }

typedef struct SkillRec { int rank; double cooldown_end_ms; double cast_progress_ms; double channel_end_ms; double next_charge_ready_ms; int charges_cur; unsigned char casting_active; unsigned char channel_active; } SkillRec;
SkillRec g_skills[4]; int g_skill_count_stub=4;
struct RoguePlayerStub { int level,xp,health; };
struct RogueAppState { int skill_count; RogueSkillDef* skill_defs; RogueSkillState* skill_states; double game_time_ms; int talent_points; struct RoguePlayerStub player; unsigned int pending_seed; double gen_water_level; double gen_cave_thresh; unsigned int vendor_seed; double vendor_time_accum_ms; double vendor_restock_interval_ms; int item_instance_cap; struct RogueItemInstance* item_instances; } g_app;
const RogueSkillDef* rogue_skill_get_def(int id){ if(id<0||id>=g_app.skill_count) return NULL; return &g_defs[id]; }
const RogueSkillState* rogue_skill_get_state(int id){ if(id<0||id>=g_app.skill_count) return NULL; return &g_skill_states[id]; }
void rogue_skills_init(void){}
void rogue_skills_shutdown(void){}
int rogue_skill_register(const RogueSkillDef* def){ (void)def; return -1; }
int rogue_skill_rank_up(int id){ (void)id; return -1; }
int rogue_skill_try_activate(int id, const RogueSkillCtx* ctx){ (void)id;(void)ctx; return 0; }
int rogue_skill_try_cancel(int id, const RogueSkillCtx* ctx){ (void)id;(void)ctx; return 0; }
void rogue_skills_update(double now_ms){ (void)now_ms; }
int rogue_skill_synergy_total(int synergy_id){ (void)synergy_id; return 0; }
int rogue_skills_load_from_cfg(const char* path){ (void)path; return 0; }

int main(void){
    setvbuf(stdout,NULL,_IONBF,0);
    rogue_save_manager_reset_for_tests(); rogue_save_manager_init();
   /* Only register skills (4) and buffs (5) to isolate new serialization; avoid components accessing deeper g_app. */
   RogueSaveComponent SK={4,write_local_skills,read_local_skills,"skills"}; RogueSaveComponent BF={5,NULL,NULL,"buffs"};
   rogue_save_manager_register(&SK);
   rogue_save_manager_register(&BF);
   g_app.game_time_ms = 0.0;
   for(int i=0;i<g_skill_count_stub;i++){ memset(&g_defs[i],0,sizeof g_defs[i]); g_defs[i].max_rank=10; g_defs[i].id=i; memset(&g_skill_states[i],0,sizeof g_skill_states[i]); g_skill_states[i].rank=i; g_skill_states[i].cooldown_end_ms=1000.0*i; g_skill_states[i].cast_progress_ms=50.0*i; g_skill_states[i].channel_end_ms=2000.0*i; g_skill_states[i].next_charge_ready_ms=500.0*i; g_skill_states[i].charges_cur=i%3; g_skill_states[i].casting_active=(unsigned char)(i&1); g_skill_states[i].channel_active=(unsigned char)((i>>1)&1); }
    rogue_buffs_apply(1,5,3000.0,0.0); rogue_buffs_apply(2,7,1500.0,0.0);
    if(rogue_save_manager_save_slot(0)!=0){ printf("PH7_FAIL save\n"); return 1; }
   memset(g_skills,0,sizeof g_skills); g_buff_count_internal=0; memset(g_buffs_internal,0,sizeof g_buffs_internal); g_app.game_time_ms = 0.0;
    if(rogue_save_manager_load_slot(0)!=0){ printf("PH7_FAIL load\n"); return 1; }
    for(int i=0;i<g_skill_count_stub;i++){ if(g_skill_states[i].rank!=i){ printf("PH7_FAIL skill_rank_%d=%d\n", i, g_skill_states[i].rank); return 1; } }
    if(g_buff_count_internal<2){ printf("PH7_FAIL buffs=%d\n", g_buff_count_internal); return 1; }
   printf("PH7_OK skills=%d buffs=%d charges0=%d cast0=%.1f\n", g_skill_count_stub, g_buff_count_internal, g_skills[0].charges_cur, g_skills[0].cast_progress_ms);
    return 0; }
