/* Skill registry & ranking management (relocated) */
#include "core/skills/skills_internal.h" /* direct include to avoid forwarding indirection issues */
#include "core/app_state.h"
#include "core/buffs.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void rogue_effect_apply(int effect_spec_id, double now_ms);
void rogue_persistence_save_player_stats(void);

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
#ifdef ROGUE_HAVE_SDL
    g_app.skill_icon_textures = NULL;
#endif
    for(int i=0;i<10;i++) g_app.skill_bar[i]=-1;
    g_app.talent_points = 0;
    for(int i=0;i<ROGUE_MAX_SYNERGIES;i++) g_synergy_totals[i]=0;
}

void rogue_skills_shutdown(void){
#ifdef ROGUE_HAVE_SDL
    if(g_app.skill_icon_textures){
        for(int i=0;i<g_skill_count_internal;i++){ rogue_texture_destroy(&g_app.skill_icon_textures[i]); }
        free(g_app.skill_icon_textures); g_app.skill_icon_textures=NULL;
    }
#endif
    /* Free duplicated strings for name/icon */
    for(int i=0;i<g_skill_count_internal;i++){
        if(g_skill_defs_internal && g_skill_defs_internal[i].name){ free((char*)g_skill_defs_internal[i].name); g_skill_defs_internal[i].name=NULL; }
        if(g_skill_defs_internal && g_skill_defs_internal[i].icon){ free((char*)g_skill_defs_internal[i].icon); g_skill_defs_internal[i].icon=NULL; }
    }
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
#ifdef ROGUE_HAVE_SDL
    /* resize icon texture array to match */
    RogueTexture* nt = (RogueTexture*)realloc(g_app.skill_icon_textures, sizeof(RogueTexture)*g_skill_count_internal);
    if(nt){ g_app.skill_icon_textures = nt; g_app.skill_icon_textures[g_skill_count_internal-1].handle=NULL; g_app.skill_icon_textures[g_skill_count_internal-1].w=0; g_app.skill_icon_textures[g_skill_count_internal-1].h=0; }
#endif
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

const RogueSkillDef* rogue_skill_get_def(int id){ if(id<0 || id>=g_skill_count_internal) return NULL; return &g_skill_defs_internal[id]; }
const RogueSkillState* rogue_skill_get_state(int id){ if(id<0 || id>=g_skill_count_internal) return NULL; return &g_skill_states_internal[id]; }

/* Data-driven loader (new format expected by assets/skills.cfg):
   name,icon_png,max_rank,base_cd_ms,cd_reduction_ms_per_rank,is_passive,tags,synergy_id,synergy_per_rank,
   resource_cost_mana,ap_cost,max_charges,charge_ms,cast_time_ms,input_buffer_ms,min_weave_ms,early_cancel_min_pct,
   cast_type,combo_builder,combo_spender,effect_spec_id
   (Fields after max_rank are optional; missing numeric values default to 0.) */
static char* rogue__strdup(const char* s){ if(!s) return NULL; size_t n=strlen(s); char* d=(char*)malloc(n+1); if(!d) return NULL; memcpy(d,s,n+1); return d; }
int rogue_skills_load_from_cfg(const char* path){
    if(!path) return 0;
    ROGUE_LOG_INFO("Loading skills cfg: %s", path);
    FILE* f=NULL;
#if defined(_MSC_VER)
    if(fopen_s(&f,path,"rb")!=0){ ROGUE_LOG_ERROR("Failed to open skills cfg: %s", path); return 0; }
#else
    f=fopen(path,"rb"); if(!f){ ROGUE_LOG_ERROR("Failed to open skills cfg: %s", path); return 0; }
#endif
    char line[1024]; int loaded=0; int lineno=0;
    while(fgets(line,sizeof line,f)){
        lineno++;
        if(line[0]=='#' || line[0]=='\n' || line[0]=='\r') continue;
        char* nl=strpbrk(line,"\r\n"); if(nl) *nl='\0';
        /* Tokenize */
        const int MAX_COLS=32; char* cols[MAX_COLS]; int colc=0; char* ctx=NULL;
#if defined(_MSC_VER)
        char* tok=strtok_s(line,",",&ctx);
        while(tok && colc<MAX_COLS){ cols[colc++]=tok; tok=strtok_s(NULL,",",&ctx); }
#else
        char* tok=strtok(line,",{}");
        while(tok && colc<MAX_COLS){ cols[colc++]=tok; tok=strtok(NULL,",{}"); }
#endif
        if(colc<1){ ROGUE_LOG_WARN("skills.cfg line %d: empty/invalid", lineno); continue; }
        ROGUE_LOG_DEBUG("skills.cfg line %d: name=%s icon=%s", lineno, cols[0], (colc>1)?cols[1]:"<none>");
        RogueSkillDef def; memset(&def,0,sizeof def); def.id=-1; def.synergy_id=-1;
        def.name = rogue__strdup(cols[0]);
        if(colc>1) def.icon = rogue__strdup(cols[1]);
        if(colc>2) def.max_rank = atoi(cols[2]); else def.max_rank=1;
        if(colc>3) def.base_cooldown_ms = (float)atof(cols[3]);
        if(colc>4) def.cooldown_reduction_ms_per_rank = (float)atof(cols[4]);
        if(colc>5) def.is_passive = atoi(cols[5]);
        if(colc>6) def.tags = atoi(cols[6]);
        if(colc>7) def.synergy_id = atoi(cols[7]);
        if(colc>8) def.synergy_value_per_rank = atoi(cols[8]);
        if(colc>9) def.resource_cost_mana = atoi(cols[9]);
        if(colc>10) def.action_point_cost = atoi(cols[10]);
        if(colc>11) def.max_charges = atoi(cols[11]);
        if(colc>12) def.charge_recharge_ms = (float)atof(cols[12]);
        if(colc>13) def.cast_time_ms = (float)atof(cols[13]);
        if(colc>14) def.input_buffer_ms = (unsigned short)atoi(cols[14]);
        if(colc>15) def.min_weave_ms = (unsigned short)atoi(cols[15]);
        if(colc>16) def.early_cancel_min_pct = (unsigned char)atoi(cols[16]);
        if(colc>17) def.cast_type = (unsigned char)atoi(cols[17]);
        if(colc>18) def.combo_builder = (unsigned char)atoi(cols[18]);
        if(colc>19) def.combo_spender = (unsigned char)atoi(cols[19]);
        if(colc>20) def.effect_spec_id = atoi(cols[20]);
        /* Register (assigns id) */
        int rid = rogue_skill_register(&def);
        if(rid>=0){
            /* Load icon texture now (if SDL available) */
#ifdef ROGUE_HAVE_SDL
            if(def.icon && g_app.skill_icon_textures){
                char full[512];
                /* If path already begins with assets/ leave as-is, otherwise prepend */
                if(strncmp(def.icon,"assets/",7)==0){ snprintf(full,sizeof full,"%s", def.icon); }
                else { snprintf(full,sizeof full,"assets/%s", def.icon); }
                if(rogue_texture_load(&g_app.skill_icon_textures[rid], full)){
                    ROGUE_LOG_INFO("Skill icon loaded (id=%d name=%s path=%s)", rid, def.name, full);
                } else {
                    ROGUE_LOG_WARN("Skill icon FAILED to load (id=%d name=%s path=%s)", rid, def.name, full);
                }
            }
#endif
            loaded++;
        } else {
            ROGUE_LOG_ERROR("Failed to register skill '%s' (line %d)", def.name?def.name:"<null>", lineno);
            free((char*)def.name); free((char*)def.icon);
        }
    }
    fclose(f);
    ROGUE_LOG_INFO("Skills loaded: %d", loaded);
    return loaded;
}
