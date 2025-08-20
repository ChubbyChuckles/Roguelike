/* Phase 2.1+ layered stat cache implementation */
#include "core/stat_cache.h"
#include <stdio.h>
#include <string.h>
#include "core/equipment.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/equipment_stats.h"
#include "core/loot_item_defs.h"
#include "core/progression_ratings.h"
#include <string.h>
#include <stddef.h>

RogueStatCache g_player_stat_cache = {0};

void rogue_stat_cache_mark_dirty(void){ g_player_stat_cache.dirty = 1; }

static int weapon_base_damage_estimate(void){
    int inst = rogue_equip_get(ROGUE_EQUIP_WEAPON);
    if(inst<0) return 3; /* fallback */
    const RogueItemInstance* it = rogue_item_instance_at(inst);
    if(!it) return 3;
    /* Base damage from rarity plus affix flat damage (prefix/suffix) */
    int base = 5 + it->rarity * 4;
    if(it->prefix_index>=0){ const RogueAffixDef* a=rogue_affix_at(it->prefix_index); if(a && a->stat==ROGUE_AFFIX_STAT_DAMAGE_FLAT) base += it->prefix_value; }
    if(it->suffix_index>=0){ const RogueAffixDef* a=rogue_affix_at(it->suffix_index); if(a && a->stat==ROGUE_AFFIX_STAT_DAMAGE_FLAT) base += it->suffix_value; }
    return base;
}

/* Aggregate defensive stat (simple armor sum) and secondary affix bonuses for derived estimates */
static int total_armor_value(void){ int sum=0; for(int s=ROGUE_EQUIP_ARMOR_HEAD;s<ROGUE_EQUIP__COUNT;s++){ /* restrict to armor-like slots */
        if(s==ROGUE_EQUIP_RING1||s==ROGUE_EQUIP_RING2||s==ROGUE_EQUIP_AMULET||s==ROGUE_EQUIP_CHARM1||s==ROGUE_EQUIP_CHARM2) continue; /* jewelry not contributing base armor */
        int inst=rogue_equip_get((enum RogueEquipSlot)s); if(inst<0) continue; const RogueItemInstance* it=rogue_item_instance_at(inst); if(!it) continue; const RogueItemDef* d=rogue_item_def_at(it->def_index); if(d) sum += d->base_armor; }
    return sum; }

/* Soft cap curve: value approaches cap asymptotically; softness controls steepness */
float rogue_soft_cap_apply(float value, float cap, float softness){ if(cap<=0.f) return value; if(softness<=0.f) softness=1.f; if(value<=cap) return value; float over = value - cap; return cap + over / (1.f + over / (cap * softness)); }

static unsigned long long fingerprint_fold(unsigned long long fp, unsigned long long v){ fp ^= v + 0x9e3779b97f4a7c15ULL + (fp<<6) + (fp>>2); return fp; }

static void compute_layers(const RoguePlayer* p){
    /* For Phase 2 we only have base layer (player stats) + affix contributions (weapon damage already indirect) */
    g_player_stat_cache.base_strength = p->strength;
    g_player_stat_cache.base_dexterity = p->dexterity;
    g_player_stat_cache.base_vitality = p->vitality;
    g_player_stat_cache.base_intelligence = p->intelligence;
    /* Implicit layer populated by equipment aggregation (Phase 4.1). Leave existing values (zero if none). */
    /* affix_* fields pre-populated by equipment aggregation pass */
    /* Leave existing affix_* values intact (do not zero) to allow external gather step before update. */
    g_player_stat_cache.buff_strength = g_player_stat_cache.buff_dexterity = 0;
    g_player_stat_cache.buff_vitality = g_player_stat_cache.buff_intelligence = 0;
    /* TODO Phase 2.3+: gather implicit & buff stats once systems exist. */
    /* unique_* layer reserved for Phase 4.2 unique item hooks; default zero if not yet populated */
    g_player_stat_cache.total_strength = g_player_stat_cache.base_strength + g_player_stat_cache.implicit_strength + g_player_stat_cache.unique_strength + g_player_stat_cache.set_strength + g_player_stat_cache.runeword_strength + g_player_stat_cache.affix_strength + g_player_stat_cache.buff_strength;
    g_player_stat_cache.total_dexterity = g_player_stat_cache.base_dexterity + g_player_stat_cache.implicit_dexterity + g_player_stat_cache.unique_dexterity + g_player_stat_cache.set_dexterity + g_player_stat_cache.runeword_dexterity + g_player_stat_cache.affix_dexterity + g_player_stat_cache.buff_dexterity;
    g_player_stat_cache.total_vitality = g_player_stat_cache.base_vitality + g_player_stat_cache.implicit_vitality + g_player_stat_cache.unique_vitality + g_player_stat_cache.set_vitality + g_player_stat_cache.runeword_vitality + g_player_stat_cache.affix_vitality + g_player_stat_cache.buff_vitality;
    g_player_stat_cache.total_intelligence = g_player_stat_cache.base_intelligence + g_player_stat_cache.implicit_intelligence + g_player_stat_cache.unique_intelligence + g_player_stat_cache.set_intelligence + g_player_stat_cache.runeword_intelligence + g_player_stat_cache.affix_intelligence + g_player_stat_cache.buff_intelligence;
    g_player_stat_cache.rating_crit = p->crit_rating; g_player_stat_cache.rating_haste = p->haste_rating; g_player_stat_cache.rating_avoidance = p->avoidance_rating;
    g_player_stat_cache.rating_crit_eff_pct = (int)(rogue_rating_effective_percent(ROGUE_RATING_CRIT, g_player_stat_cache.rating_crit)+0.5f);
    g_player_stat_cache.rating_haste_eff_pct = (int)(rogue_rating_effective_percent(ROGUE_RATING_HASTE, g_player_stat_cache.rating_haste)+0.5f);
    g_player_stat_cache.rating_avoidance_eff_pct = (int)(rogue_rating_effective_percent(ROGUE_RATING_AVOIDANCE, g_player_stat_cache.rating_avoidance)+0.5f);
    /* Resist layers currently only from affix layer (future: implicit, buffs). Leave existing if externally populated. */
    if(g_player_stat_cache.resist_physical < 0) g_player_stat_cache.resist_physical=0;
    if(g_player_stat_cache.resist_fire < 0) g_player_stat_cache.resist_fire=0;
    if(g_player_stat_cache.resist_cold < 0) g_player_stat_cache.resist_cold=0;
    if(g_player_stat_cache.resist_lightning < 0) g_player_stat_cache.resist_lightning=0;
    if(g_player_stat_cache.resist_poison < 0) g_player_stat_cache.resist_poison=0;
    if(g_player_stat_cache.resist_status < 0) g_player_stat_cache.resist_status=0;
}

/* ---- Phase 11 Analytics Implementation ---- */
typedef struct EquipHistBin { int count; long long dps_sum; long long ehp_sum; } EquipHistBin;
/* rarity (0-4) x slot count (reuse ROGUE_EQUIP_SLOT_COUNT if available else 16) */
#ifndef ROGUE_EQUIP_SLOT_COUNT
#define ROGUE_EQUIP_SLOT_COUNT 16
#endif
static EquipHistBin g_hist[5][ROGUE_EQUIP_SLOT_COUNT];
/* Basic rolling DPS samples for outlier detection (weapon rarity only). */
static int g_dps_samples[256]; static int g_dps_sample_count=0; static int g_dps_sample_pos=0; static int g_weapon_rarity_last=0;
/* Phase 11.3: Usage tracking (simple fixed-size accumulators). */
#define ROGUE_ANALYTICS_SET_CAP 64
static int g_set_ids[ROGUE_ANALYTICS_SET_CAP];
static int g_set_counts[ROGUE_ANALYTICS_SET_CAP];
static int g_set_unique_count=0;
#define ROGUE_ANALYTICS_UNIQUE_CAP 128
static int g_unique_base_defs[ROGUE_ANALYTICS_UNIQUE_CAP]; /* store base item def indices for uniques */
static int g_unique_counts[ROGUE_ANALYTICS_UNIQUE_CAP];
static int g_unique_used=0;

int rogue_equipment_stats_export_json(char* buf, int cap){
    if(!buf || cap<=0) return -1; int w = snprintf(buf, (size_t)cap,
        "{\"dps\":%d,\"ehp\":%d,\"mobility\":%d,\"strength\":%d,\"dexterity\":%d,\"vitality\":%d,\"intelligence\":%d}",
        g_player_stat_cache.dps_estimate, g_player_stat_cache.ehp_estimate, g_player_stat_cache.mobility_index,
        g_player_stat_cache.total_strength, g_player_stat_cache.total_dexterity, g_player_stat_cache.total_vitality, g_player_stat_cache.total_intelligence);
    if(w >= cap) return -1; return w; }

/* Forward decls to query equipment; keep header deps minimal */
int rogue_equip_get(enum RogueEquipSlot slot);
const RogueItemInstance* rogue_item_instance_at(int index);
const RogueItemDef* rogue_item_def_at(int index);

void rogue_equipment_histogram_record(void){
    for(int slot=0; slot<ROGUE_EQUIP_SLOT_COUNT; ++slot){ int inst = rogue_equip_get((enum RogueEquipSlot)slot); if(inst<0) continue; const RogueItemInstance* it = rogue_item_instance_at(inst); if(!it) continue; int rarity = it->rarity; if(rarity<0||rarity>4) continue; EquipHistBin* b = &g_hist[rarity][slot]; b->count++; b->dps_sum += g_player_stat_cache.dps_estimate; b->ehp_sum += g_player_stat_cache.ehp_estimate; if(slot==ROGUE_EQUIP_WEAPON){ g_weapon_rarity_last = rarity; if(g_dps_sample_count < (int)(sizeof g_dps_samples/sizeof g_dps_samples[0])){ g_dps_samples[g_dps_sample_count++] = g_player_stat_cache.dps_estimate; } else { g_dps_samples[g_dps_sample_pos] = g_player_stat_cache.dps_estimate; g_dps_sample_pos = (g_dps_sample_pos+1) % g_dps_sample_count; } } }
}

int rogue_equipment_histograms_export_json(char* buf, int cap){
    if(!buf || cap<=2) return -1; int off=0; buf[off++]='{' ;
    for(int r=0;r<5;r++){
        for(int s=0;s<ROGUE_EQUIP_SLOT_COUNT;s++){
            EquipHistBin* b=&g_hist[r][s]; if(b->count==0) continue; int avg_dps = (int)(b->dps_sum / b->count); int avg_ehp = (int)(b->ehp_sum / b->count);
            int n = snprintf(buf+off, (size_t)(cap-off), "\"r%d_s%d\":{\"count\":%d,\"avg_dps\":%d,\"avg_ehp\":%d},", r,s,b->count,avg_dps,avg_ehp); if(n<0 || n >= cap-off) return -1; off+=n; }
    }
    if(off>1 && buf[off-1]==',') off--; buf[off++]='}'; if(off<cap) buf[off]='\0'; else return -1; return off;
}

int rogue_equipment_dps_outlier_flag(void){
    if(g_dps_sample_count < 8) return 0; /* need baseline */
    /* Compute median and MAD (median absolute deviation) */
    int tmp[256]; memcpy(tmp,g_dps_samples,(size_t)g_dps_sample_count*sizeof(int)); /* naive nth_element substitute: simple insertion sort small N */
    for(int i=1;i<g_dps_sample_count;i++){ int v=tmp[i]; int j=i-1; while(j>=0 && tmp[j]>v){ tmp[j+1]=tmp[j]; j--; } tmp[j+1]=v; }
    int median = tmp[g_dps_sample_count/2];
    for(int i=0;i<g_dps_sample_count;i++){ int d = g_dps_samples[i]-median; if(d<0) d=-d; tmp[i]=d; }
    for(int i=1;i<g_dps_sample_count;i++){ int v=tmp[i]; int j=i-1; while(j>=0 && tmp[j]>v){ tmp[j+1]=tmp[j]; j--; } tmp[j+1]=v; }
    int mad = tmp[g_dps_sample_count/2]; if(mad<=0) mad=1;
    int latest = g_player_stat_cache.dps_estimate; int deviation = latest - median; if(deviation<0) deviation = -deviation; /* flag if > 5 * MAD */
    if(deviation > 5 * mad) return 1; return 0;
}

/* Forward decls for usage tracking */
const RogueItemDef* rogue_item_def_at(int index);
int rogue_unique_find_by_base_def(int def_index);

void rogue_equipment_usage_record(void){
    /* Iterate equipped items; accumulate set id counts and unique base occurrences. */
    for(int slot=0; slot<ROGUE_EQUIP_SLOT_COUNT; ++slot){
        int inst = rogue_equip_get((enum RogueEquipSlot)slot); if(inst<0) continue; const RogueItemInstance* it = rogue_item_instance_at(inst); if(!it) continue; const RogueItemDef* d = rogue_item_def_at(it->def_index); if(!d) continue;
        if(d->set_id>0){ /* accumulate */
            int found=-1; for(int i=0;i<g_set_unique_count;i++){ if(g_set_ids[i]==d->set_id){ found=i; break; } }
            if(found<0){ if(g_set_unique_count<ROGUE_ANALYTICS_SET_CAP){ g_set_ids[g_set_unique_count]=d->set_id; g_set_counts[g_set_unique_count]=1; g_set_unique_count++; }
            } else { g_set_counts[found]++; }
        }
        /* Unique base detection via registry lookup */
        if(rogue_unique_find_by_base_def(it->def_index)>=0){ int found=-1; for(int i=0;i<g_unique_used;i++){ if(g_unique_base_defs[i]==it->def_index){ found=i; break; } }
            if(found<0){ if(g_unique_used<ROGUE_ANALYTICS_UNIQUE_CAP){ g_unique_base_defs[g_unique_used]=it->def_index; g_unique_counts[g_unique_used]=1; g_unique_used++; }
            } else { g_unique_counts[found]++; }
        }
    }
}

int rogue_equipment_usage_export_json(char* buf, int cap){
    if(!buf||cap<2) return -1; int off=0; buf[off++]='{';
    for(int i=0;i<g_set_unique_count;i++){ int n=snprintf(buf+off,(size_t)(cap-off),"\"set_%d\":%d,", g_set_ids[i], g_set_counts[i]); if(n<0||n>=cap-off) return -1; off+=n; }
    for(int i=0;i<g_unique_used;i++){ int n=snprintf(buf+off,(size_t)(cap-off),"\"unique_%d\":%d,", g_unique_base_defs[i], g_unique_counts[i]); if(n<0||n>=cap-off) return -1; off+=n; }
    if(off>1 && buf[off-1]==',') off--; buf[off++]='}'; if(off<cap) buf[off]='\0'; else return -1; return off; }

static void compute_derived(const RoguePlayer* p){
    int base_weapon = weapon_base_damage_estimate();
    int armor_total = total_armor_value();
    float dex_scalar = 1.0f + (float)g_player_stat_cache.total_dexterity / 50.0f;
    float crit_mult = 1.0f + (p->crit_chance/100.0f) * (p->crit_damage/100.0f);
    g_player_stat_cache.dps_estimate = (int)(base_weapon * dex_scalar * crit_mult);
    int max_hp = p->max_health + armor_total * 2;
    float vit_scalar = 1.0f + (float)g_player_stat_cache.total_vitality / 200.0f;
    g_player_stat_cache.ehp_estimate = (int)(max_hp * vit_scalar);
    if(g_player_stat_cache.ehp_estimate < max_hp) g_player_stat_cache.ehp_estimate = max_hp;
    g_player_stat_cache.toughness_index = g_player_stat_cache.ehp_estimate; /* placeholder */
    g_player_stat_cache.mobility_index = (int)(100 + g_player_stat_cache.total_dexterity * 1.5f); /* simple scalar baseline */
    g_player_stat_cache.sustain_index = 0; /* no life-steal implemented yet */
    /* Apply soft cap at 75% with diminishing above (hard cap 90%). */
    const float soft_cap=75.f; const float softness=0.65f; const int hard_cap=90;
    int* res_array[] = { &g_player_stat_cache.resist_physical,&g_player_stat_cache.resist_fire,&g_player_stat_cache.resist_cold,&g_player_stat_cache.resist_lightning,&g_player_stat_cache.resist_poison,&g_player_stat_cache.resist_status };
    for(size_t i=0;i<sizeof(res_array)/sizeof(res_array[0]);++i){ int v=*res_array[i]; if(v> (int)soft_cap){ float adj=rogue_soft_cap_apply((float)v,soft_cap,softness); v=(int)(adj+0.5f); } if(v>hard_cap) v=hard_cap; *res_array[i]=v; if(v<0) *res_array[i]=0; }
}

static void compute_fingerprint(void){
    unsigned long long fp=0xcbf29ce484222325ULL; /* FNV offset basis variant seed */
    const int* ints = (const int*)&g_player_stat_cache;
    /* Only include layered + derived numeric fields before fingerprint & dirty itself (exclude fingerprint, dirty) */
    size_t count = offsetof(RogueStatCache,fingerprint)/sizeof(int); /* number of int fields preceding fingerprint */
    for(size_t i=0;i<count;i++){ fp = fingerprint_fold(fp, (unsigned long long)(unsigned int)ints[i]); }
    g_player_stat_cache.fingerprint = fp;
}

void rogue_stat_cache_update(const RoguePlayer* p){ if(!p) return; if(!g_player_stat_cache.dirty) return; rogue_stat_cache_force_update(p); }

void rogue_stat_cache_force_update(const RoguePlayer* p){ if(!p) return; compute_layers(p); compute_derived(p); compute_fingerprint(); g_player_stat_cache.dirty = 0; }

unsigned long long rogue_stat_cache_fingerprint(void){ return g_player_stat_cache.fingerprint; }
