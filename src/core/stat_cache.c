/* Phase 2.1+ layered stat cache implementation */
#include "core/stat_cache.h"
#include "core/equipment.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/equipment_stats.h"
#include "core/loot_item_defs.h"
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
    g_player_stat_cache.implicit_strength = g_player_stat_cache.implicit_dexterity = 0;
    g_player_stat_cache.implicit_vitality = g_player_stat_cache.implicit_intelligence = 0;
    /* affix_* fields may have been pre-populated by equipment aggregation pass */
    /* Leave existing affix_* values intact (do not zero) to allow external gather step before update. */
    g_player_stat_cache.buff_strength = g_player_stat_cache.buff_dexterity = 0;
    g_player_stat_cache.buff_vitality = g_player_stat_cache.buff_intelligence = 0;
    /* TODO Phase 2.3+: gather implicit & buff stats once systems exist. */
    g_player_stat_cache.total_strength = g_player_stat_cache.base_strength + g_player_stat_cache.implicit_strength + g_player_stat_cache.affix_strength + g_player_stat_cache.buff_strength;
    g_player_stat_cache.total_dexterity = g_player_stat_cache.base_dexterity + g_player_stat_cache.implicit_dexterity + g_player_stat_cache.affix_dexterity + g_player_stat_cache.buff_dexterity;
    g_player_stat_cache.total_vitality = g_player_stat_cache.base_vitality + g_player_stat_cache.implicit_vitality + g_player_stat_cache.affix_vitality + g_player_stat_cache.buff_vitality;
    g_player_stat_cache.total_intelligence = g_player_stat_cache.base_intelligence + g_player_stat_cache.implicit_intelligence + g_player_stat_cache.affix_intelligence + g_player_stat_cache.buff_intelligence;
    /* Resist layers currently only from affix layer (future: implicit, buffs). Leave existing if externally populated. */
    if(g_player_stat_cache.resist_physical < 0) g_player_stat_cache.resist_physical=0;
    if(g_player_stat_cache.resist_fire < 0) g_player_stat_cache.resist_fire=0;
    if(g_player_stat_cache.resist_cold < 0) g_player_stat_cache.resist_cold=0;
    if(g_player_stat_cache.resist_lightning < 0) g_player_stat_cache.resist_lightning=0;
    if(g_player_stat_cache.resist_poison < 0) g_player_stat_cache.resist_poison=0;
    if(g_player_stat_cache.resist_status < 0) g_player_stat_cache.resist_status=0;
}

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
