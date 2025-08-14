#include "core/equipment_stats.h"
#include "core/equipment.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/stat_cache.h"

/* Internal helper: sum agility affix values across equipped items. */
static int gather_agility_bonus(void){
    int total=0;
    for(int slot=0; slot<ROGUE_EQUIP__COUNT; ++slot){
        int inst_index = rogue_equip_get((enum RogueEquipSlot)slot);
        if(inst_index<0) continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst_index);
        if(!it) continue;
        if(it->prefix_index>=0){ const RogueAffixDef* a=rogue_affix_at(it->prefix_index); if(a && a->stat==ROGUE_AFFIX_STAT_AGILITY_FLAT) total += it->prefix_value; }
        if(it->suffix_index>=0){ const RogueAffixDef* a=rogue_affix_at(it->suffix_index); if(a && a->stat==ROGUE_AFFIX_STAT_AGILITY_FLAT) total += it->suffix_value; }
    }
    return total;
}

void rogue_equipment_apply_stat_bonuses(RoguePlayer* p){
    if(!p) return;
    /* Base player stats already in p (strength, dexterity, etc.). Apply additive bonuses. */
    int agility_bonus = gather_agility_bonus();
    p->dexterity += agility_bonus;
    if(p->dexterity < 0) p->dexterity = 0; /* safety */
    if(p->dexterity > 9999) p->dexterity = 9999;
    rogue_stat_cache_mark_dirty();
}
