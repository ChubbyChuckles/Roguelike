/* enemy_difficulty_scaling.c - Phase 1 implementation (baseline scaling + ΔL model) */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "core/enemy_difficulty_scaling.h"
#include "core/enemy_adaptive.h"

#ifndef ROGUE_CLAMP
#define ROGUE_CLAMP(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
#endif

/* Default parameter set (tunable via file). */
static RogueEnemyDifficultyParams g_params = {
    /* d_def d_dmg cap_def cap_dmg u_def u_dmg u_cap_def u_cap_dmg ramp_soft dom thr trivial thr reward scalar */
    0.05f, 0.04f, 0.60f, 0.55f, 0.06f, 0.05f, 2.50f, 2.20f, 0.30f, 8, 12, 0.15f
};

const RogueEnemyDifficultyParams* rogue_enemy_difficulty_params_current(void){ return &g_params; }
void rogue_enemy_difficulty_params_reset(void){
    g_params = (RogueEnemyDifficultyParams){0.05f,0.04f,0.60f,0.55f,0.06f,0.05f,2.50f,2.20f,0.30f,8,12,0.15f};
}

/* Simple key=value parser (whitespace trimmed, lines starting with # ignored) */
int rogue_enemy_difficulty_load_params_file(const char* path){
    FILE* f = fopen(path, "rb");
    if(!f) return -1; char line[256];
    while(fgets(line,sizeof(line),f)){
        char* p=line; while(*p==' '||*p=='\t') ++p; if(*p=='#' || *p=='\n' || *p=='\0') continue;
        char key[64]; float value=0.f; if(sscanf(p,"%63[^=]=%f", key, &value)==2){
            if(strcmp(key,"d_def")==0) g_params.d_def=value; else if(strcmp(key,"d_dmg")==0) g_params.d_dmg=value;
            else if(strcmp(key,"cap_def")==0) g_params.cap_def=value; else if(strcmp(key,"cap_dmg")==0) g_params.cap_dmg=value;
            else if(strcmp(key,"u_def")==0) g_params.u_def=value; else if(strcmp(key,"u_dmg")==0) g_params.u_dmg=value;
            else if(strcmp(key,"u_cap_def")==0) g_params.u_cap_def=value; else if(strcmp(key,"u_cap_dmg")==0) g_params.u_cap_dmg=value;
            else if(strcmp(key,"ramp_soft")==0) g_params.ramp_soft=value; else if(strcmp(key,"dominance_threshold")==0) g_params.dominance_threshold=(int)value;
            else if(strcmp(key,"trivial_threshold")==0) g_params.trivial_threshold=(int)value; else if(strcmp(key,"reward_trivial_scalar")==0) g_params.reward_trivial_scalar=value;
        }
    }
    fclose(f); return 0;
}

/* Sublinear base curves: chosen simple forms (future phases can hot swap).
 * HP: grows ~ L^1.15; Damage: L^1.08; Defense: L^1.05 (milder) */
float rogue_enemy_base_hp(int enemy_level){ if(enemy_level<1) enemy_level=1; return 100.f * powf((float)enemy_level, 1.15f); }
float rogue_enemy_base_damage(int enemy_level){ if(enemy_level<1) enemy_level=1; return 12.f * powf((float)enemy_level, 1.08f); }
float rogue_enemy_base_defense(int enemy_level){ if(enemy_level<1) enemy_level=1; return 8.f * powf((float)enemy_level, 1.05f); }

RogueEnemyBaseStats rogue_enemy_base_stats(int enemy_level){
    RogueEnemyBaseStats s = { rogue_enemy_base_hp(enemy_level), rogue_enemy_base_damage(enemy_level), rogue_enemy_base_defense(enemy_level) };
    return s;
}

int rogue_enemy_difficulty_internal__relative_multipliers(int player_level, int enemy_level, float* out_hp_mult, float* out_dmg_mult){
    if(player_level<1||enemy_level<1) return -1; if(!out_hp_mult||!out_dmg_mult) return -2;
    int dL = player_level - enemy_level; const RogueEnemyDifficultyParams* p=&g_params;
    float hp_mult=1.f; float dmg_mult=1.f;
    if(dL==0){ /* same level */ }
    else if(dL>0){ /* player over-level -> penalties */
        float down_hp = dL * p->d_def; float down_dmg = dL * p->d_dmg;
        if(down_hp > p->cap_def) down_hp = p->cap_def; if(down_dmg > p->cap_dmg) down_dmg = p->cap_dmg;
        hp_mult = 1.f - down_hp; if(hp_mult < 0.05f) hp_mult=0.05f; /* never 0 */
        dmg_mult = 1.f - down_dmg; if(dmg_mult < 0.05f) dmg_mult=0.05f;
    } else { /* dL < 0 => enemy over-level -> buffs */
        int adL = -dL;
        float up_hp = adL * p->u_def - p->ramp_soft; if(up_hp < 0.f) up_hp = 0.f; if(up_hp > p->u_cap_def) up_hp = p->u_cap_def;
        float up_dmg = adL * p->u_dmg - p->ramp_soft; if(up_dmg < 0.f) up_dmg = 0.f; if(up_dmg > p->u_cap_dmg) up_dmg = p->u_cap_dmg;
        hp_mult = 1.f + up_hp; dmg_mult = 1.f + up_dmg;
    }
    *out_hp_mult = hp_mult; *out_dmg_mult = dmg_mult; return 0;
}

int rogue_enemy_compute_final_stats(int player_level, int enemy_level, int tier_id, RogueEnemyFinalStats* out){
    if(!out) return -3; RogueEnemyBaseStats base = rogue_enemy_base_stats(enemy_level);
    const RogueEnemyTierDesc* tier = rogue_enemy_tier_get(tier_id); if(!tier) return -2;
    float rel_hp_mult=1.f, rel_dmg_mult=1.f; if(rogue_enemy_difficulty_internal__relative_multipliers(player_level, enemy_level, &rel_hp_mult, &rel_dmg_mult)!=0) return -1;
    /* Defense uses hp path for relative scaling (could diverge later). */
    float tier_hp = base.hp * tier->mult.hp_budget;
    float tier_dmg = base.damage * tier->mult.dps_budget;
    float tier_def = base.defense * tier->mult.hp_budget; /* approximate survivability tie */
    float final_hp = tier_hp * rel_hp_mult;
    float final_dmg = tier_dmg * rel_dmg_mult;
    float final_def = tier_def * rel_hp_mult;
    /* Adaptive scalar applies uniformly post-tier & relative scaling */
    float adapt = rogue_enemy_adaptive_scalar(); if(adapt<0.01f) adapt=0.01f; final_hp*=adapt; final_dmg*=adapt; final_def*=adapt;
    out->hp=final_hp; out->damage=final_dmg; out->defense=final_def;
    out->hp_mult = rel_hp_mult * tier->mult.hp_budget; out->dmg_mult = rel_dmg_mult * tier->mult.dps_budget; out->def_mult = rel_hp_mult * tier->mult.hp_budget;
    return 0;
}

float rogue_enemy_compute_reward_scalar(int player_level, int enemy_level, float modifier_complexity_score, float adaptive_state_scalar){
    (void)modifier_complexity_score; (void)adaptive_state_scalar; const RogueEnemyDifficultyParams* p=&g_params;
    int dL = player_level - enemy_level; if(dL >= p->trivial_threshold) return p->reward_trivial_scalar;
    if(dL <= 0) return 1.f; /* equal or under-level => full rewards */
    /* Linear interpolation between dominance_threshold (full) and trivial_threshold (min) */
    if(dL <= p->dominance_threshold) return 1.f;
    float span = (float)(p->trivial_threshold - p->dominance_threshold); if(span <= 0.f) return 1.f; float t = (float)(dL - p->dominance_threshold)/span;
    if(t<0.f) t=0.f; if(t>1.f) t=1.f; return 1.f - t*(1.f - p->reward_trivial_scalar);
}

/* ---------------- Biome parameter registry (Phase 1.4 extension) ---------------- */
typedef struct { int biome_id; RogueEnemyDifficultyParams params; } BiomeParamEntry;
#define MAX_BIOME_PARAMS 32
static BiomeParamEntry g_biome_params[MAX_BIOME_PARAMS];
static int g_biome_param_count = 0;

int rogue_enemy_difficulty_register_biome_params(int biome_id, const RogueEnemyDifficultyParams* params){
    if(!params) return -2; if(biome_id<0) return -3;
    for(int i=0;i<g_biome_param_count;i++) if(g_biome_params[i].biome_id==biome_id){ g_biome_params[i].params=*params; return 0; }
    if(g_biome_param_count>=MAX_BIOME_PARAMS) return -1;
    g_biome_params[g_biome_param_count].biome_id=biome_id; g_biome_params[g_biome_param_count].params=*params; g_biome_param_count++; return 0;
}

const RogueEnemyDifficultyParams* rogue_enemy_difficulty_params_for_biome(int biome_id){
    for(int i=0;i<g_biome_param_count;i++) if(g_biome_params[i].biome_id==biome_id) return &g_biome_params[i].params;
    return NULL;
}

/* Helper selecting params (biome override else global). */
static const RogueEnemyDifficultyParams* _select_params(int biome_id){
    const RogueEnemyDifficultyParams* b = (biome_id>=0)? rogue_enemy_difficulty_params_for_biome(biome_id):NULL;
    return b? b : &g_params;
}

/* ΔL severity classification (Phase 1.6 placeholder) */
RogueEnemyDeltaLSeverity rogue_enemy_difficulty_classify_delta(int player_level, int enemy_level){
    const RogueEnemyDifficultyParams* p=&g_params; int dL = player_level - enemy_level;
    if(dL==0) return ROGUE_DLVL_EQUAL;
    if(dL>0){
        if(dL >= p->trivial_threshold) return ROGUE_DLVL_TRIVIAL;
        if(dL >= p->dominance_threshold) return ROGUE_DLVL_DOMINANCE;
        if(dL >= 5) return ROGUE_DLVL_MAJOR; /* heuristic */
        return ROGUE_DLVL_MINOR;
    }
    /* Enemy higher level */
    if(-dL >= 8) return ROGUE_DLVL_MAJOR; if(-dL >=4) return ROGUE_DLVL_MODERATE; return ROGUE_DLVL_MINOR;
}

/* Attribute curves (Phase 1.3) – lightweight proxies until full rating system.
 * - crit_chance: starts 2%, grows log-slow with level & DPS budget
 * - phys_resist: scales with hp budget and level^0.6 (capped 60%)
 * - elem_resist: slightly below phys for baseline (capped 55%) */
int rogue_enemy_difficulty_internal__attrib_curves(int enemy_level, float hp_budget, float dps_budget, RogueEnemyDerivedAttributes* out){
    if(!out) return -1; if(enemy_level<1) enemy_level=1;
    float L = (float)enemy_level;
    float crit = 0.02f + 0.12f * (logf(L+1.f)/logf(101.f)) * (0.5f + 0.5f * (dps_budget));
    if(crit>0.30f) crit=0.30f;
    float phys = 0.05f + 0.65f * powf(L,0.60f)/powf(100.f,0.60f) * (0.4f + 0.6f * (hp_budget));
    if(phys>0.60f) phys=0.60f;
    float elem = phys * 0.92f; if(elem>0.55f) elem=0.55f;
    out->crit_chance=crit; out->phys_resist=phys; out->elem_resist=elem; return 0;
}

int rogue_enemy_compute_final_stats_biome(int player_level, int enemy_level, int tier_id, int biome_id, RogueEnemyFinalStats* out){
    if(!out) return -3; RogueEnemyBaseStats base = rogue_enemy_base_stats(enemy_level);
    const RogueEnemyTierDesc* tier = rogue_enemy_tier_get(tier_id); if(!tier) return -2;
    float rel_hp_mult=1.f, rel_dmg_mult=1.f; if(rogue_enemy_difficulty_internal__relative_multipliers(player_level, enemy_level, &rel_hp_mult, &rel_dmg_mult)!=0) return -1;
    const RogueEnemyDifficultyParams* p = _select_params(biome_id); (void)p; /* future biome-specific adjustments could apply here */
    float tier_hp = base.hp * tier->mult.hp_budget;
    float tier_dmg = base.damage * tier->mult.dps_budget;
    float tier_def = base.defense * tier->mult.hp_budget;
    float adapt = rogue_enemy_adaptive_scalar(); if(adapt<0.01f) adapt=0.01f;
    out->hp = tier_hp * rel_hp_mult * adapt;
    out->damage = tier_dmg * rel_dmg_mult * adapt;
    out->defense = tier_def * rel_hp_mult * adapt;
    out->hp_mult = rel_hp_mult * tier->mult.hp_budget;
    out->dmg_mult = rel_dmg_mult * tier->mult.dps_budget;
    out->def_mult = rel_hp_mult * tier->mult.hp_budget;
    return 0;
}

int rogue_enemy_compute_attributes(int player_level, int enemy_level, int tier_id, int biome_id, RogueEnemyDerivedAttributes* out){
    (void)player_level; (void)biome_id; /* reserved for future adaptive & biome-specific ordering */
    const RogueEnemyTierDesc* tier = rogue_enemy_tier_get(tier_id); if(!tier) return -2;
    return rogue_enemy_difficulty_internal__attrib_curves(enemy_level, tier->mult.hp_budget, tier->mult.dps_budget, out);
}

float rogue_enemy_estimate_ttk_seconds(int player_level, int enemy_level, int tier_id, int biome_id, float player_dps){
    if(player_dps <= 0.f) return -1.f; RogueEnemyFinalStats fs; if(rogue_enemy_compute_final_stats_biome(player_level, enemy_level, tier_id, biome_id, &fs)!=0) return -2.f;
    /* Very simple: effective HP / player_dps (defense converts ~ linearly reduction proxy). */
    float defense_factor = 1.f + fs.defense / 500.f; /* diminishing contribution */
    float ehp = fs.hp * defense_factor;
    return ehp / player_dps;
}
