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
    int last_sequence;
} RogueProcState;

static RogueProcState g_proc_states[ROGUE_PROC_CAP];
static int g_proc_count=0;
/* Track global time & trigger rate (already declared further down in file in original context; ensure single definitions). */

/* Forward declare utility to iterate active shield procs */
static int is_shield_proc(const RogueProcState* st){ if(!st) return 0; return st->def.trigger == ROGUE_PROC_ON_BLOCK && st->stacks>0 && st->duration_remaining>0; }

int rogue_procs_absorb_pool(void){ int total=0; for(int i=0;i<g_proc_count;i++){ const RogueProcState* st=&g_proc_states[i]; if(is_shield_proc(st)){ total += st->def.magnitude * st->stacks; }} return total; }
int rogue_procs_consume_absorb(int amount){ if(amount<=0) return 0; int remaining=amount; for(int i=0;i<g_proc_count && remaining>0;i++){ RogueProcState* st=&g_proc_states[i]; if(!is_shield_proc(st)) continue; int avail = st->def.magnitude * st->stacks; if(avail<=0) continue; int take = (avail<remaining?avail:remaining); /* simplistic: consume whole proc if any taken */ remaining -= take; /* reduce stacks accordingly */ int per_stack = st->def.magnitude; if(per_stack>0){ int stacks_to_remove = (take + per_stack -1)/per_stack; st->stacks -= stacks_to_remove; if(st->stacks < 0) st->stacks=0; if(st->stacks==0){ st->duration_remaining=0; } }
    }
    return amount - remaining; }
int rogue_proc_force_activate(int id, int stacks, int duration_ms){ if(id<0 || id>=g_proc_count) return -1; RogueProcState* st=&g_proc_states[id]; st->stacks = stacks; if(st->stacks<0) st->stacks=0; st->duration_remaining = duration_ms; if(st->duration_remaining<0) st->duration_remaining=0; return 0; }
static int g_time_accum_ms=0;
static int g_rate_cap_per_sec = 1000; /* effectively uncapped default */
static int g_triggers_this_second=0;
static int g_global_sequence=0; /* deterministic ordering */

static void tick_second_window(int dt_ms){
    g_time_accum_ms += dt_ms;
    if(g_time_accum_ms>=1000){ g_time_accum_ms -= 1000; g_triggers_this_second = 0; }
}

int rogue_proc_register(const RogueProcDef* def){
    if(!def) return -1; if(g_proc_count>=ROGUE_PROC_CAP) return -2; RogueProcState* st=&g_proc_states[g_proc_count]; memset(st,0,sizeof *st); st->def=*def; st->def.id=g_proc_count; st->registered=1; st->icd_remaining=0; st->duration_remaining=0; st->stacks=0; st->trigger_count=0; st->active_time_ms=0; st->last_sequence=0; g_proc_count++; return st->def.id; }

void rogue_procs_reset(void){ memset(g_proc_states,0,sizeof g_proc_states); g_proc_count=0; g_time_accum_ms=0; g_triggers_this_second=0; g_global_sequence=0; }

static void maybe_add_active_time(int dt_ms){ for(int i=0;i<g_proc_count;i++){ RogueProcState* st=&g_proc_states[i]; if(st->duration_remaining>0 && st->stacks>0){ st->active_time_ms += dt_ms; } } }

void rogue_procs_update(int dt_ms, int hp_cur, int hp_max){ (void)hp_cur; (void)hp_max; tick_second_window(dt_ms); maybe_add_active_time(dt_ms); for(int i=0;i<g_proc_count;i++){ RogueProcState* st=&g_proc_states[i]; if(st->icd_remaining>0){ st->icd_remaining -= dt_ms; if(st->icd_remaining<0) st->icd_remaining=0; } if(st->duration_remaining>0){ st->duration_remaining -= dt_ms; if(st->duration_remaining<=0){ st->duration_remaining=0; st->stacks=0; } } } }

void rogue_proc_set_rate_cap_per_sec(int cap){ g_rate_cap_per_sec = cap>0?cap:1; }

static void trigger_proc(RogueProcState* st){
    if(!st) return; if(st->icd_remaining>0) return; if(g_triggers_this_second>=g_rate_cap_per_sec) return;
    st->icd_remaining = st->def.icd_ms;
    st->trigger_count++; g_triggers_this_second++; st->last_sequence = ++g_global_sequence;
    if(st->def.duration_ms>0){
        switch(st->def.stack_rule){
            case ROGUE_PROC_STACK_STACK:
                if(st->stacks<st->def.max_stacks || st->def.max_stacks<=0) st->stacks++;
                if(st->duration_remaining<=0) st->duration_remaining = st->def.duration_ms; /* first stack start */
                break;
            case ROGUE_PROC_STACK_REFRESH:
                st->stacks = st->stacks>0?st->stacks:1;
                st->duration_remaining = st->def.duration_ms;
                break;
            case ROGUE_PROC_STACK_IGNORE:
            default:
                if(st->stacks==0){ st->stacks=1; st->duration_remaining = st->def.duration_ms; }
                break;
        }
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
float rogue_proc_triggers_per_min(int id){ if(id<0||id>=g_proc_count) return 0.f; if(g_time_accum_ms<=0) return 0.f; float minutes = (float)g_time_accum_ms/60000.f; if(minutes<=0.f) return 0.f; return (float)g_proc_states[id].trigger_count / minutes; }
int rogue_proc_last_trigger_sequence(int id){ if(id<0||id>=g_proc_count) return 0; return g_proc_states[id].last_sequence; }
int rogue_proc_count(void){ return g_proc_count; }
const RogueProcDef* rogue_proc_def(int id){ if(id<0||id>=g_proc_count) return 0; return &g_proc_states[id].def; }
