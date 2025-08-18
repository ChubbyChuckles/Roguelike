#include "core/equipment_stats.h"
#include "core/equipment.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/stat_cache.h"

/* Collect affix-derived flat bonuses across all equipped items and populate stat cache affix_* fields.
   This supports Equipment Phase 2.1+ layered aggregation without mutating base player stats directly. */
static void gather_affix_primary_and_armor(void){
    int str=0,dex=0,vit=0,intel=0,armor=0;
    for(int slot=0; slot<ROGUE_EQUIP__COUNT; ++slot){
        int inst_index = rogue_equip_get((enum RogueEquipSlot)slot);
        if(inst_index<0) continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst_index);
        if(!it) continue;
        if(it->prefix_index>=0){ const RogueAffixDef* a=rogue_affix_at(it->prefix_index); if(a){
            switch(a->stat){
                case ROGUE_AFFIX_STAT_STRENGTH_FLAT: str += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_DEXTERITY_FLAT: dex += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_VITALITY_FLAT: vit += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_INTELLIGENCE_FLAT: intel += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_ARMOR_FLAT: armor += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_AGILITY_FLAT: dex += it->prefix_value; break; /* legacy */
                default: break;
            }
        }}
        if(it->suffix_index>=0){ const RogueAffixDef* a=rogue_affix_at(it->suffix_index); if(a){
            switch(a->stat){
                case ROGUE_AFFIX_STAT_STRENGTH_FLAT: str += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_DEXTERITY_FLAT: dex += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_VITALITY_FLAT: vit += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_INTELLIGENCE_FLAT: intel += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_ARMOR_FLAT: armor += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_AGILITY_FLAT: dex += it->suffix_value; break; /* legacy */
                default: break;
            }
        }}
    }
    g_player_stat_cache.affix_strength = str;
    g_player_stat_cache.affix_dexterity = dex;
    g_player_stat_cache.affix_vitality = vit;
    g_player_stat_cache.affix_intelligence = intel;
    g_player_stat_cache.affix_armor_flat = armor; /* new extension field (added below if not present) */
}

void rogue_equipment_apply_stat_bonuses(RoguePlayer* p){
    (void)p; /* player base unchanged here; layering system pulls from p + cache additions */
    gather_affix_primary_and_armor();
    rogue_stat_cache_mark_dirty();
}
