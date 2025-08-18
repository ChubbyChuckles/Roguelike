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
    int r_phys=0,r_fire=0,r_cold=0,r_light=0,r_poison=0,r_status=0;
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
                case ROGUE_AFFIX_STAT_RESIST_PHYSICAL: r_phys += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_FIRE: r_fire += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_COLD: r_cold += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_LIGHTNING: r_light += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_POISON: r_poison += it->prefix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_STATUS: r_status += it->prefix_value; break;
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
                case ROGUE_AFFIX_STAT_RESIST_PHYSICAL: r_phys += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_FIRE: r_fire += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_COLD: r_cold += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_LIGHTNING: r_light += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_POISON: r_poison += it->suffix_value; break;
                case ROGUE_AFFIX_STAT_RESIST_STATUS: r_status += it->suffix_value; break;
                default: break;
            }
        }}
    }
    g_player_stat_cache.affix_strength = str;
    g_player_stat_cache.affix_dexterity = dex;
    g_player_stat_cache.affix_vitality = vit;
    g_player_stat_cache.affix_intelligence = intel;
    g_player_stat_cache.affix_armor_flat = armor; /* new extension field (added below if not present) */
    /* Store raw resistance sums (percentage points). */
    g_player_stat_cache.resist_physical = r_phys;
    g_player_stat_cache.resist_fire = r_fire;
    g_player_stat_cache.resist_cold = r_cold;
    g_player_stat_cache.resist_lightning = r_light;
    g_player_stat_cache.resist_poison = r_poison;
    g_player_stat_cache.resist_status = r_status;
}

void rogue_equipment_apply_stat_bonuses(RoguePlayer* p){
    (void)p; /* player base unchanged here; layering system pulls from p + cache additions */
    gather_affix_primary_and_armor();
    rogue_stat_cache_mark_dirty();
}
