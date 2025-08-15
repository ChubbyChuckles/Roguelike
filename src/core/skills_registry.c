/* Skill registry & ranking management (extracted from monolithic skills.c) */
#include "core/skills_internal.h"
#include "core/app_state.h"
#include "core/buffs.h"
#include <stdlib.h>
#include <string.h>

/* Forward (effect_spec.c) */
void rogue_effect_apply(int effect_spec_id, double now_ms);
/* Forward (persistence.c) */
void rogue_persistence_save_player_stats(void);

/* Former static globals */
struct RogueSkillDef* g_skill_defs_internal = NULL;
struct RogueSkillState* g_skill_states_internal = NULL;
int g_skill_capacity_internal = 0;
int g_skill_count_internal = 0;

#define ROGUE_MAX_SYNERGIES 16
static int g_synergy_totals[ROGUE_MAX_SYNERGIES];

void rogue_skills_recompute_synergies(void){
    for(int i=0;i<ROGUE_MAX_SYNERGIES;i++) g_synergy_totals[i]=0;
    for(int i=0;i<g_skill_count_internal;i++){
        const RogueSkillDef* d=&g_skill_defs_internal[i]; const RogueSkillState* st=&g_skill_states_internal[i];
        if(d->is_passive && d->synergy_id>=0 && d->synergy_id<ROGUE_MAX_SYNERGIES){
            g_synergy_totals[d->synergy_id] += st->rank * d->synergy_value_per_rank;
        }
    }
}

void rogue_skills_ensure_capacity(int min_cap){
    if(g_skill_capacity_internal >= min_cap) return;
    int new_cap = g_skill_capacity_internal? g_skill_capacity_internal*2:8; if(new_cap<min_cap) new_cap=min_cap;
    RogueSkillDef* nd = (RogueSkillDef*)realloc(g_skill_defs_internal, (size_t)new_cap*sizeof(RogueSkillDef)); if(!nd) return; g_skill_defs_internal=nd;
    RogueSkillState* ns = (RogueSkillState*)realloc(g_skill_states_internal, (size_t)new_cap*sizeof(RogueSkillState)); if(!ns) return; g_skill_states_internal=ns;
    for(int i=g_skill_capacity_internal;i<new_cap;i++){ RogueSkillState* s=&g_skill_states_internal[i]; memset(s,0,sizeof *s); }
    g_skill_capacity_internal = new_cap;
}

void rogue_skills_init(void){
    g_skill_defs_internal=NULL; g_skill_states_internal=NULL; g_skill_capacity_internal=0; g_skill_count_internal=0;
    g_app.skill_defs = NULL; g_app.skill_states=NULL; g_app.skill_count=0;
    for(int i=0;i<10;i++) g_app.skill_bar[i]=-1;
    g_app.talent_points = 0;
    for(int i=0;i<ROGUE_MAX_SYNERGIES;i++) g_synergy_totals[i]=0;
}

void rogue_skills_shutdown(void){
    free(g_skill_defs_internal); g_skill_defs_internal=NULL;
    free(g_skill_states_internal); g_skill_states_internal=NULL;
    g_skill_capacity_internal=0; g_skill_count_internal=0;
    g_app.skill_defs=NULL; g_app.skill_states=NULL; g_app.skill_count=0;
}

int rogue_skill_register(const RogueSkillDef* def){
    if(!def) return -1;
    rogue_skills_ensure_capacity(g_skill_count_internal+1);
    RogueSkillDef* d = &g_skill_defs_internal[g_skill_count_internal];
    *d = *def; d->id = g_skill_count_internal;
    RogueSkillState* st=&g_skill_states_internal[g_skill_count_internal]; memset(st,0,sizeof *st);
    st->charges_cur = d->max_charges>0?d->max_charges:0;
    g_skill_count_internal++;
    g_app.skill_defs = g_skill_defs_internal; g_app.skill_states = g_skill_states_internal; g_app.skill_count=g_skill_count_internal;
    return d->id;
}

int rogue_skill_rank_up(int id){
    if(id<0 || id>=g_skill_count_internal) return -1;
    RogueSkillState* st = &g_skill_states_internal[id];
    const RogueSkillDef* def = &g_skill_defs_internal[id];
    if(st->rank >= def->max_rank) return st->rank;
    if(g_app.talent_points<=0) return -1;
    st->rank++; g_app.talent_points--; g_app.stats_dirty=1; rogue_skills_recompute_synergies();
    rogue_persistence_save_player_stats();
    return st->rank;
}

int rogue_skill_synergy_total(int synergy_id){ if(synergy_id<0 || synergy_id>=ROGUE_MAX_SYNERGIES) return 0; return g_synergy_totals[synergy_id]; }

/* Query passthroughs */
const RogueSkillDef* rogue_skill_get_def(int id){ if(id<0 || id>=g_skill_count_internal) return NULL; return &g_skill_defs_internal[id]; }
const RogueSkillState* rogue_skill_get_state(int id){ if(id<0 || id>=g_skill_count_internal) return NULL; return &g_skill_states_internal[id]; }
