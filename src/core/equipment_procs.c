#include "core/equipment_procs.h"
#include <string.h>

#ifndef ROGUE_PROC_CAP
#define ROGUE_PROC_CAP 64
#endif

typedef struct RogueProcState {
    RogueProcDef def;
    int registered;
    int icd_remaining;
    int duration_remaining;
    int stacks;
    int trigger_count;
    int active_time_ms;
} RogueProcState;

static RogueProcState g_proc_states[ROGUE_PROC_CAP];
static int g_proc_count=0;
static int g_time_accum_ms=0;
static int g_rate_cap_per_sec = 1000; /* effectively uncapped default */
static int g_triggers_this_second=0;

static void tick_second_window(int dt_ms){
    g_time_accum_ms += dt_ms;
    if(g_time_accum_ms>=1000){ g_time_accum_ms -= 1000; g_triggers_this_second = 0; }
}

int rogue_proc_register(const RogueProcDef* def){
    if(!def) return -1; if(g_proc_count>=ROGUE_PROC_CAP) return -2; RogueProcState* st=&g_proc_states[g_proc_count]; memset(st,0,sizeof *st); st->def=*def; st->def.id=g_proc_count; st->registered=1; st->icd_remaining=0; st->duration_remaining=0; st->stacks=0; st->trigger_count=0; st->active_time_ms=0; g_proc_count++; return st->def.id; }

void rogue_procs_reset(void){ memset(g_proc_states,0,sizeof g_proc_states); g_proc_count=0; g_time_accum_ms=0; g_triggers_this_second=0; }

static void maybe_add_active_time(int dt_ms){ for(int i=0;i<g_proc_count;i++){ RogueProcState* st=&g_proc_states[i]; if(st->duration_remaining>0 && st->stacks>0){ st->active_time_ms += dt_ms; } } }

void rogue_procs_update(int dt_ms, int hp_cur, int hp_max){ (void)hp_cur; (void)hp_max; tick_second_window(dt_ms); maybe_add_active_time(dt_ms); for(int i=0;i<g_proc_count;i++){ RogueProcState* st=&g_proc_states[i]; if(st->icd_remaining>0){ st->icd_remaining -= dt_ms; if(st->icd_remaining<0) st->icd_remaining=0; } if(st->duration_remaining>0){ st->duration_remaining -= dt_ms; if(st->duration_remaining<=0){ st->duration_remaining=0; st->stacks=0; } } } }

void rogue_proc_set_rate_cap_per_sec(int cap){ g_rate_cap_per_sec = cap>0?cap:1; }

static void trigger_proc(RogueProcState* st){ if(!st) return; if(st->icd_remaining>0) return; if(g_triggers_this_second>=g_rate_cap_per_sec) return; st->icd_remaining = st->def.icd_ms; st->trigger_count++; g_triggers_this_second++; if(st->def.duration_ms>0){ if(st->def.stack_rule==ROGUE_PROC_STACK_STACK){ if(st->stacks<st->def.max_stacks || st->def.max_stacks<=0) st->stacks++; } else if(st->def.stack_rule==ROGUE_PROC_STACK_REFRESH){ st->stacks = st->stacks>0?st->stacks:1; st->duration_remaining = st->def.duration_ms; } else { if(st->stacks==0){ st->stacks=1; st->duration_remaining = st->def.duration_ms; } }
        if(st->def.stack_rule==ROGUE_PROC_STACK_REFRESH){ st->duration_remaining = st->def.duration_ms; }
        if(st->def.stack_rule==ROGUE_PROC_STACK_STACK){ st->duration_remaining = st->def.duration_ms; }
    }
}

static void handle_trigger(RogueProcTrigger trig){ for(int i=0;i<g_proc_count;i++){ RogueProcState* st=&g_proc_states[i]; if(st->def.trigger==trig) trigger_proc(st); } }

void rogue_procs_event_hit(int was_crit){ handle_trigger(ROGUE_PROC_ON_HIT); if(was_crit) handle_trigger(ROGUE_PROC_ON_CRIT); }
void rogue_procs_event_kill(void){ handle_trigger(ROGUE_PROC_ON_KILL); }
void rogue_procs_event_block(void){ handle_trigger(ROGUE_PROC_ON_BLOCK); }
void rogue_procs_event_dodge(void){ handle_trigger(ROGUE_PROC_ON_DODGE); }

int rogue_proc_trigger_count(int id){ if(id<0||id>=g_proc_count) return 0; return g_proc_states[id].trigger_count; }
int rogue_proc_active_stacks(int id){ if(id<0||id>=g_proc_count) return 0; return g_proc_states[id].stacks; }
float rogue_proc_uptime_ratio(int id){ if(id<0||id>=g_proc_count) return 0.f; if(g_time_accum_ms<=0) return 0.f; return (float)g_proc_states[id].active_time_ms / (float) ( (g_time_accum_ms) ); }
