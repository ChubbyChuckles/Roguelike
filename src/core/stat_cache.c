#include "core/stat_cache.h"
#include "core/equipment.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/equipment_stats.h"

RogueStatCache g_player_stat_cache = {0,0,0,0,0,0,1};

void rogue_stat_cache_mark_dirty(void){ g_player_stat_cache.dirty = 1; }

static int weapon_base_damage_estimate(void){
    int inst = rogue_equip_get(ROGUE_EQUIP_WEAPON);
    if(inst<0) return 3; /* fallback */
    const RogueItemInstance* it = rogue_item_instance_at(inst);
    if(!it) return 3;
    /* Simple: rarity scales base damage */
    int base = 5 + it->rarity * 4;
    return base;
}

void rogue_stat_cache_update(const RoguePlayer* p){
    if(!p) return; if(!g_player_stat_cache.dirty) return;
    g_player_stat_cache.total_strength = p->strength;
    g_player_stat_cache.total_dexterity = p->dexterity;
    g_player_stat_cache.total_vitality = p->vitality;
    g_player_stat_cache.total_intelligence = p->intelligence;
    int base_weapon = weapon_base_damage_estimate();
    /* DPS heuristic: base weapon * (1 + dex/50) * (1 + crit_chance*crit_damage_factor) */
    float dex_scalar = 1.0f + (float)g_player_stat_cache.total_dexterity / 50.0f;
    float crit_mult = 1.0f + (p->crit_chance/100.0f) * (p->crit_damage/100.0f);
    g_player_stat_cache.dps_estimate = (int)(base_weapon * dex_scalar * crit_mult);
    /* EHP heuristic: max_health * (1 + vitality/200) */
    int max_hp = p->max_health;
    float vit_scalar = 1.0f + (float)g_player_stat_cache.total_vitality / 200.0f;
    g_player_stat_cache.ehp_estimate = (int)(max_hp * vit_scalar);
    if(g_player_stat_cache.ehp_estimate < max_hp) g_player_stat_cache.ehp_estimate = max_hp;
    g_player_stat_cache.dirty = 0;
}
