#include "core/equipment_stats.h"
#include "core/equipment.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/stat_cache.h"
#include "core/equipment_uniques.h"
/* Phase 4.1: Implicit + set bonus + runeword scaffolding
    For Phase 4.1 we populate only implicits; future sub-phases will extend with unique/set/runeword layers. */

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

/* Gather implicit stats from base item definitions of equipped items. These feed the implicit_* layer in stat cache. */
static void gather_implicit_primary_and_armor(void){
    int str=0,dex=0,vit=0,intel=0,armor_flat=0;
    int r_phys=0,r_fire=0,r_cold=0,r_light=0,r_poison=0,r_status=0;
    for(int slot=0; slot<ROGUE_EQUIP__COUNT; ++slot){
        int inst_index = rogue_equip_get((enum RogueEquipSlot)slot);
        if(inst_index<0) continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst_index);
        if(!it) continue;
        const RogueItemDef* d = rogue_item_def_at(it->def_index);
        if(!d) continue;
        str += d->implicit_strength;
        dex += d->implicit_dexterity;
        vit += d->implicit_vitality;
        intel += d->implicit_intelligence;
        armor_flat += d->implicit_armor_flat;
        r_phys += d->implicit_resist_physical;
        r_fire += d->implicit_resist_fire;
        r_cold += d->implicit_resist_cold;
        r_light += d->implicit_resist_lightning;
        r_poison += d->implicit_resist_poison;
        r_status += d->implicit_resist_status;
    }
    g_player_stat_cache.implicit_strength = str;
    g_player_stat_cache.implicit_dexterity = dex;
    g_player_stat_cache.implicit_vitality = vit;
    g_player_stat_cache.implicit_intelligence = intel;
    /* For armor/resists, implicit layer contributes directly to aggregate resist fields (currently single-layer).
       We add to existing sums so affix and implicit sources combine. */
    g_player_stat_cache.affix_armor_flat += armor_flat; /* reuse flat armor field for now (no separate implicit armor field) */
    g_player_stat_cache.resist_physical += r_phys;
    g_player_stat_cache.resist_fire += r_fire;
    g_player_stat_cache.resist_cold += r_cold;
    g_player_stat_cache.resist_lightning += r_light;
    g_player_stat_cache.resist_poison += r_poison;
    g_player_stat_cache.resist_status += r_status;
}

/* Gather unique item fixed bonuses into unique_* layer (Phase 4.2). */
static void gather_unique_primary(void){
    int str=0,dex=0,vit=0,intel=0; int armor_flat=0; int r_phys=0,r_fire=0,r_cold=0,r_light=0,r_poison=0,r_status=0;
    for(int slot=0; slot<ROGUE_EQUIP__COUNT; ++slot){
        int inst_index = rogue_equip_get((enum RogueEquipSlot)slot); if(inst_index<0) continue; const RogueItemInstance* it=rogue_item_instance_at(inst_index); if(!it) continue; const RogueItemDef* d=rogue_item_def_at(it->def_index); if(!d) continue; int uidx = rogue_unique_find_by_base_def(it->def_index); if(uidx<0) continue; const RogueUniqueDef* u = rogue_unique_at(uidx); if(!u) continue; str += u->strength; dex += u->dexterity; vit += u->vitality; intel += u->intelligence; armor_flat += u->armor_flat; r_phys += u->resist_physical; r_fire += u->resist_fire; r_cold += u->resist_cold; r_light += u->resist_lightning; r_poison += u->resist_poison; r_status += u->resist_status; }
    g_player_stat_cache.unique_strength = str;
    g_player_stat_cache.unique_dexterity = dex;
    g_player_stat_cache.unique_vitality = vit;
    g_player_stat_cache.unique_intelligence = intel;
    g_player_stat_cache.affix_armor_flat += armor_flat; /* fold into shared flat armor contribution */
    g_player_stat_cache.resist_physical += r_phys; g_player_stat_cache.resist_fire += r_fire; g_player_stat_cache.resist_cold += r_cold; g_player_stat_cache.resist_lightning += r_light; g_player_stat_cache.resist_poison += r_poison; g_player_stat_cache.resist_status += r_status;
}

void rogue_equipment_apply_stat_bonuses(RoguePlayer* p){
    (void)p; /* player base unchanged here; layering system pulls from p + cache additions */
    /* Reset dynamic aggregation fields we own before recomputing. Base & implicit fields cleared in stat cache compute_layers. */
    g_player_stat_cache.affix_strength = g_player_stat_cache.affix_dexterity = 0;
    g_player_stat_cache.affix_vitality = g_player_stat_cache.affix_intelligence = 0;
    g_player_stat_cache.affix_armor_flat = 0;
    /* Resist fields reset to 0 before summing from affix + implicit layers. */
    g_player_stat_cache.resist_physical = g_player_stat_cache.resist_fire = 0;
    g_player_stat_cache.resist_cold = g_player_stat_cache.resist_lightning = 0;
    g_player_stat_cache.resist_poison = g_player_stat_cache.resist_status = 0;
    gather_affix_primary_and_armor();
    gather_implicit_primary_and_armor();
    gather_unique_primary();
    rogue_stat_cache_mark_dirty();
}
