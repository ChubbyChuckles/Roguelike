#include "core/skills.h"
#include "core/app_state.h"
#include <stdlib.h>
#include <string.h>

/* Simple dynamic array for skill defs */
static RogueSkillDef* g_defs = NULL;
static RogueSkillState* g_states = NULL;
static int g_capacity = 0;
static int g_count = 0;

static void ensure_capacity(int min_cap){
    if(g_capacity >= min_cap) return;
    int new_cap = g_capacity? g_capacity*2:8; if(new_cap<min_cap) new_cap=min_cap;
    RogueSkillDef* nd = (RogueSkillDef*)realloc(g_defs, (size_t)new_cap*sizeof(RogueSkillDef));
    if(!nd) return; g_defs=nd;
    RogueSkillState* ns = (RogueSkillState*)realloc(g_states, (size_t)new_cap*sizeof(RogueSkillState));
    if(!ns) return; g_states=ns;
    for(int i=g_capacity;i<new_cap;i++){ g_states[i].rank=0; g_states[i].cooldown_end_ms=0; g_states[i].uses=0; }
    g_capacity = new_cap;
}

void rogue_skills_init(void){
    g_defs=NULL; g_states=NULL; g_capacity=0; g_count=0;
    g_app.skill_defs = NULL; g_app.skill_states=NULL; g_app.skill_count=0;
    for(int i=0;i<10;i++) g_app.skill_bar[i]=-1;
    g_app.talent_points = 0;
}

void rogue_skills_shutdown(void){
    free(g_defs); g_defs=NULL;
    free(g_states); g_states=NULL;
    g_capacity=0; g_count=0;
    g_app.skill_defs=NULL; g_app.skill_states=NULL; g_app.skill_count=0;
}

int rogue_skill_register(const RogueSkillDef* def){
    if(!def) return -1;
    ensure_capacity(g_count+1);
    RogueSkillDef* d = &g_defs[g_count];
    *d = *def; /* shallow copy (icon/name assumed static) */
    d->id = g_count;
    g_states[g_count].rank=0; g_states[g_count].cooldown_end_ms=0; g_states[g_count].uses=0;
    g_count++;
    g_app.skill_defs = g_defs; g_app.skill_states = g_states; g_app.skill_count=g_count;
    return d->id;
}

int rogue_skill_rank_up(int id){
    if(id<0 || id>=g_count) return -1;
    RogueSkillState* st = &g_states[id];
    const RogueSkillDef* def = &g_defs[id];
    if(st->rank >= def->max_rank) return st->rank;
    if(g_app.talent_points<=0) return -1;
    st->rank++; g_app.talent_points--; g_app.stats_dirty=1; return st->rank;
}

int rogue_skill_try_activate(int id, const RogueSkillCtx* ctx){
    if(id<0 || id>=g_count) return 0;
    RogueSkillState* st = &g_states[id];
    const RogueSkillDef* def = &g_defs[id];
    if(st->rank<=0) return 0; /* locked */
    if(ctx && ctx->now_ms < st->cooldown_end_ms) return 0;
    float cd = def->base_cooldown_ms - (st->rank-1)*def->cooldown_reduction_ms_per_rank; if(cd<100) cd=100;
    RogueSkillCtx local_ctx = ctx? *ctx : (RogueSkillCtx){0};
    int consumed = 1;
    if(def->on_activate){ consumed = def->on_activate(def, st, &local_ctx); }
    if(consumed){ st->cooldown_end_ms = (ctx? ctx->now_ms : 0.0) + cd; st->uses++; }
    return consumed;
}

void rogue_skills_update(double now_ms){
    (void)now_ms; /* placeholder for future tick logic */
}

const RogueSkillDef* rogue_skill_get_def(int id){ if(id<0 || id>=g_count) return NULL; return &g_defs[id]; }
const RogueSkillState* rogue_skill_get_state(int id){ if(id<0 || id>=g_count) return NULL; return &g_states[id]; }
