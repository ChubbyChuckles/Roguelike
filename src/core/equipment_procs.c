#include "core/equipment_procs.h"
#include <string.h>
#include <stdlib.h> /* malloc, free, strtol */
#include <stdio.h>  /* FILE, fopen_s, fseek, ftell, fread, fclose, snprintf */

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

/* ---------------- Phase 16.3 Proc Designer JSON Tooling ---------------- */
int rogue_proc_validate(const RogueProcDef* def){ if(!def) return -1; if(def->trigger < 0 || def->trigger >= ROGUE_PROC__TRIGGER_COUNT) return -2; if(def->icd_ms < 0 || def->duration_ms < 0) return -3; if(def->max_stacks < 0 || def->max_stacks > 50) return -4; if(def->stack_rule < 0 || def->stack_rule > ROGUE_PROC_STACK_IGNORE) return -5; return 0; }

static const char* ws(const char* s){ while(*s && (unsigned char)*s<=32) ++s; return s; }
static const char* jstring(const char* s,char* out,int cap){ s=ws(s); if(*s!='"') return NULL; ++s; int i=0; while(*s && *s!='"'){ if(*s=='\\'&&s[1]) s++; if(i+1<cap) out[i++]=*s; ++s;} if(*s!='"') return NULL; out[i]='\0'; return s+1; }
static const char* jnumber(const char* s,int* out){ s=ws(s); char* end=NULL; long v=strtol(s,&end,10); if(end==s) return NULL; *out=(int)v; return end; }
static int parse_trigger(const char* str){ if(strcmp(str,"ON_HIT")==0) return ROGUE_PROC_ON_HIT; if(strcmp(str,"ON_CRIT")==0) return ROGUE_PROC_ON_CRIT; if(strcmp(str,"ON_KILL")==0) return ROGUE_PROC_ON_KILL; if(strcmp(str,"ON_BLOCK")==0) return ROGUE_PROC_ON_BLOCK; if(strcmp(str,"ON_DODGE")==0) return ROGUE_PROC_ON_DODGE; if(strcmp(str,"WHEN_LOW_HP")==0) return ROGUE_PROC_WHEN_LOW_HP; return -1; }
static int parse_stack_rule(const char* str){ if(strcmp(str,"REFRESH")==0) return ROGUE_PROC_STACK_REFRESH; if(strcmp(str,"STACK")==0) return ROGUE_PROC_STACK_STACK; if(strcmp(str,"IGNORE")==0) return ROGUE_PROC_STACK_IGNORE; return -1; }

int rogue_procs_load_from_json(const char* path){ if(!path) return -1; FILE* f=NULL; 
#if defined(_MSC_VER)
 if(fopen_s(&f,path,"rb")!=0) f=NULL;
#else
 f=fopen(path,"rb");
#endif
 if(!f) return -1; fseek(f,0,SEEK_END); long sz=ftell(f); if(sz<0){ fclose(f); return -1;} fseek(f,0,SEEK_SET); char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); return -1;} size_t rd=fread(buf,1,(size_t)sz,f); buf[rd]='\0'; fclose(f); const char* s=ws(buf); if(*s!='['){ free(buf); return -1;} ++s; char key[64]; int added=0; while(1){ s=ws(s); if(*s==']'){ ++s; break; } if(*s!='{') break; ++s; RogueProcDef def; memset(&def,0,sizeof def); while(1){ s=ws(s); if(*s=='}'){ ++s; break; } const char* ns=jstring(s,key,sizeof key); if(!ns){ s++; continue; } s=ws(ns); if(*s!=':'){ free(buf); return added; } ++s; if(strcmp(key,"name")==0){ char nbuf[64]; const char* adv=jstring(s,nbuf,sizeof nbuf); if(!adv){ free(buf); return added; } int ni=0; while(nbuf[ni] && ni < (int)sizeof(def.name)-1){ def.name[ni]=nbuf[ni]; ++ni; } def.name[ni]='\0'; s=adv; }
            else if(strcmp(key,"trigger")==0){ char tbuf[32]; const char* ts=jstring(s,tbuf,sizeof tbuf); if(!ts){ free(buf); return added; } def.trigger = (RogueProcTrigger)parse_trigger(tbuf); s=ts; }
            else if(strcmp(key,"stack_rule")==0){ char sbuf[32]; const char* ss=jstring(s,sbuf,sizeof sbuf); if(!ss){ free(buf); return added; } def.stack_rule=(RogueProcStackRule)parse_stack_rule(sbuf); s=ss; }
            else { int v; const char* vs=jnumber(s,&v); if(!vs){ free(buf); return added; } if(strcmp(key,"icd_ms")==0) def.icd_ms=v; else if(strcmp(key,"duration_ms")==0) def.duration_ms=v; else if(strcmp(key,"magnitude")==0) def.magnitude=v; else if(strcmp(key,"max_stacks")==0) def.max_stacks=v; else if(strcmp(key,"param")==0) def.param=v; s=vs; }
            s=ws(s); if(*s==','){ ++s; continue; }
        }
    if(rogue_proc_validate(&def)==0){ if(rogue_proc_register(&def)>=0) added++; }
        s=ws(s); if(*s==','){ ++s; continue; }
    }
    free(buf); return added; }

int rogue_procs_export_json(char* buf,int cap){ if(!buf||cap<4) return -1; int off=0; buf[off++]='['; for(int i=0;i<g_proc_count;i++){ if(off+2>=cap) return -1; if(i>0) buf[off++]=','; char tmp[256]; const RogueProcDef* d=&g_proc_states[i].def; const char* trig="UNKNOWN"; switch(d->trigger){ case ROGUE_PROC_ON_HIT: trig="ON_HIT"; break; case ROGUE_PROC_ON_CRIT: trig="ON_CRIT"; break; case ROGUE_PROC_ON_KILL: trig="ON_KILL"; break; case ROGUE_PROC_ON_BLOCK: trig="ON_BLOCK"; break; case ROGUE_PROC_ON_DODGE: trig="ON_DODGE"; break; case ROGUE_PROC_WHEN_LOW_HP: trig="WHEN_LOW_HP"; break; default: break; } const char* sr="REFRESH"; switch(d->stack_rule){ case ROGUE_PROC_STACK_REFRESH: sr="REFRESH"; break; case ROGUE_PROC_STACK_STACK: sr="STACK"; break; case ROGUE_PROC_STACK_IGNORE: sr="IGNORE"; break; }
        int n=snprintf(tmp,sizeof tmp,"{\"name\":\"%s\",\"trigger\":\"%s\",\"icd_ms\":%d,\"duration_ms\":%d,\"magnitude\":%d,\"max_stacks\":%d,\"stack_rule\":\"%s\",\"param\":%d}", d->name, trig, d->icd_ms, d->duration_ms, d->magnitude, d->max_stacks, sr, d->param); if(n<0||off+n>=cap) return -1; memcpy(buf+off,tmp,(size_t)n); off+=n; }
 if(off+2>=cap) return -1; buf[off++]=']'; buf[off]='\0'; return off; }
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
