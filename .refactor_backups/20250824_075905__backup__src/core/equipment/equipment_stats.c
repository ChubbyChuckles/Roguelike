#include "equipment_stats.h"
#include "../loot/loot_affixes.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include "../stat_cache.h"
#include "equipment.h"
#include "equipment_content.h" /* Phase 16.2 external set/runeword registries */
#include "equipment_gems.h"    /* Phase 5.2 gem aggregation */
#include "equipment_uniques.h"

/* Placeholder: determine runeword pattern for an item instance (Phase 4.5).
    For now, reuse item id as pattern key. */
static const RogueRuneword* item_runeword(const RogueItemDef* d)
{
    return rogue_runeword_find(d ? d->id : NULL);
}
/* Phase 4.1: Implicit + set bonus + runeword scaffolding
    For Phase 4.1 we populate only implicits; future sub-phases will extend with unique/set/runeword
   layers. */

/* Collect affix-derived flat bonuses across all equipped items and populate stat cache affix_*
   fields. This supports Equipment Phase 2.1+ layered aggregation without mutating base player stats
   directly. */
static void gather_affix_primary_and_armor(void)
{
    int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
    int r_phys = 0, r_fire = 0, r_cold = 0, r_light = 0, r_poison = 0, r_status = 0;
    int block_chance = 0, block_value = 0;
    int phys_conv_fire = 0, phys_conv_frost = 0, phys_conv_arc = 0;
    int guard_recovery = 0;
    int thorns_pct = 0, thorns_cap = 0;
    for (int slot = 0; slot < ROGUE_EQUIP__COUNT; ++slot)
    {
        int inst_index = rogue_equip_get((enum RogueEquipSlot) slot);
        if (inst_index < 0)
            continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst_index);
        if (!it)
            continue;
        if (it->prefix_index >= 0)
        {
            const RogueAffixDef* a = rogue_affix_at(it->prefix_index);
            if (a)
            {
                switch (a->stat)
                {
                case ROGUE_AFFIX_STAT_STRENGTH_FLAT:
                    str += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_DEXTERITY_FLAT:
                    dex += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_VITALITY_FLAT:
                    vit += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_INTELLIGENCE_FLAT:
                    intel += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_ARMOR_FLAT:
                    armor += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_AGILITY_FLAT:
                    dex += it->prefix_value;
                    break; /* legacy */
                case ROGUE_AFFIX_STAT_RESIST_PHYSICAL:
                    r_phys += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_FIRE:
                    r_fire += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_COLD:
                    r_cold += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_LIGHTNING:
                    r_light += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_POISON:
                    r_poison += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_STATUS:
                    r_status += it->prefix_value;
                    break;
                case ROGUE_AFFIX_STAT_BLOCK_CHANCE:
                    block_chance += it->prefix_value;
                    break; /* Phase 7 */
                case ROGUE_AFFIX_STAT_BLOCK_VALUE:
                    block_value += it->prefix_value;
                    break; /* Phase 7 */
                case ROGUE_AFFIX_STAT_PHYS_CONV_FIRE_PCT:
                    phys_conv_fire += it->prefix_value;
                    break; /* 7.2 */
                case ROGUE_AFFIX_STAT_PHYS_CONV_FROST_PCT:
                    phys_conv_frost += it->prefix_value;
                    break; /* 7.2 */
                case ROGUE_AFFIX_STAT_PHYS_CONV_ARCANE_PCT:
                    phys_conv_arc += it->prefix_value;
                    break; /* 7.2 */
                case ROGUE_AFFIX_STAT_GUARD_RECOVERY_PCT:
                    guard_recovery += it->prefix_value;
                    break; /* 7.3 */
                case ROGUE_AFFIX_STAT_THORNS_PERCENT:
                    thorns_pct += it->prefix_value;
                    break; /* 7.5 */
                case ROGUE_AFFIX_STAT_THORNS_CAP:
                    thorns_cap += it->prefix_value;
                    break; /* 7.5 */
                default:
                    break;
                }
            }
        }
        if (it->suffix_index >= 0)
        {
            const RogueAffixDef* a = rogue_affix_at(it->suffix_index);
            if (a)
            {
                switch (a->stat)
                {
                case ROGUE_AFFIX_STAT_STRENGTH_FLAT:
                    str += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_DEXTERITY_FLAT:
                    dex += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_VITALITY_FLAT:
                    vit += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_INTELLIGENCE_FLAT:
                    intel += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_ARMOR_FLAT:
                    armor += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_AGILITY_FLAT:
                    dex += it->suffix_value;
                    break; /* legacy */
                case ROGUE_AFFIX_STAT_RESIST_PHYSICAL:
                    r_phys += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_FIRE:
                    r_fire += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_COLD:
                    r_cold += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_LIGHTNING:
                    r_light += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_POISON:
                    r_poison += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_RESIST_STATUS:
                    r_status += it->suffix_value;
                    break;
                case ROGUE_AFFIX_STAT_BLOCK_CHANCE:
                    block_chance += it->suffix_value;
                    break; /* Phase 7 */
                case ROGUE_AFFIX_STAT_BLOCK_VALUE:
                    block_value += it->suffix_value;
                    break; /* Phase 7 */
                case ROGUE_AFFIX_STAT_PHYS_CONV_FIRE_PCT:
                    phys_conv_fire += it->suffix_value;
                    break; /* 7.2 */
                case ROGUE_AFFIX_STAT_PHYS_CONV_FROST_PCT:
                    phys_conv_frost += it->suffix_value;
                    break; /* 7.2 */
                case ROGUE_AFFIX_STAT_PHYS_CONV_ARCANE_PCT:
                    phys_conv_arc += it->suffix_value;
                    break; /* 7.2 */
                case ROGUE_AFFIX_STAT_GUARD_RECOVERY_PCT:
                    guard_recovery += it->suffix_value;
                    break; /* 7.3 */
                case ROGUE_AFFIX_STAT_THORNS_PERCENT:
                    thorns_pct += it->suffix_value;
                    break; /* 7.5 */
                case ROGUE_AFFIX_STAT_THORNS_CAP:
                    thorns_cap += it->suffix_value;
                    break; /* 7.5 */
                default:
                    break;
                }
            }
        }
    }
    g_player_stat_cache.affix_strength = str;
    g_player_stat_cache.affix_dexterity = dex;
    g_player_stat_cache.affix_vitality = vit;
    g_player_stat_cache.affix_intelligence = intel;
    g_player_stat_cache.affix_armor_flat =
        armor; /* new extension field (added below if not present) */
    /* Store raw resistance sums (percentage points). */
    g_player_stat_cache.resist_physical = r_phys;
    g_player_stat_cache.resist_fire = r_fire;
    g_player_stat_cache.resist_cold = r_cold;
    g_player_stat_cache.resist_lightning = r_light;
    g_player_stat_cache.resist_poison = r_poison;
    g_player_stat_cache.resist_status = r_status;
    /* Phase 7 defensive extensions (currently zero until affix/stat sources defined) */
    g_player_stat_cache.block_chance += block_chance;
    g_player_stat_cache.block_value += block_value;
    g_player_stat_cache.phys_conv_fire_pct += phys_conv_fire;
    g_player_stat_cache.phys_conv_frost_pct += phys_conv_frost;
    g_player_stat_cache.phys_conv_arcane_pct += phys_conv_arc;
    g_player_stat_cache.guard_recovery_pct += guard_recovery;
    g_player_stat_cache.thorns_percent += thorns_pct;
    g_player_stat_cache.thorns_cap += thorns_cap;
}

/* Gather implicit stats from base item definitions of equipped items. These feed the implicit_*
 * layer in stat cache. */
static void gather_implicit_primary_and_armor(void)
{
    int str = 0, dex = 0, vit = 0, intel = 0, armor_flat = 0;
    int r_phys = 0, r_fire = 0, r_cold = 0, r_light = 0, r_poison = 0, r_status = 0;
    for (int slot = 0; slot < ROGUE_EQUIP__COUNT; ++slot)
    {
        int inst_index = rogue_equip_get((enum RogueEquipSlot) slot);
        if (inst_index < 0)
            continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst_index);
        if (!it)
            continue;
        const RogueItemDef* d = rogue_item_def_at(it->def_index);
        if (!d)
            continue;
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
    /* For armor/resists, implicit layer contributes directly to aggregate resist fields (currently
       single-layer). We add to existing sums so affix and implicit sources combine. */
    g_player_stat_cache.affix_armor_flat +=
        armor_flat; /* reuse flat armor field for now (no separate implicit armor field) */
    g_player_stat_cache.resist_physical += r_phys;
    g_player_stat_cache.resist_fire += r_fire;
    g_player_stat_cache.resist_cold += r_cold;
    g_player_stat_cache.resist_lightning += r_light;
    g_player_stat_cache.resist_poison += r_poison;
    g_player_stat_cache.resist_status += r_status;
}

/* Gather unique item fixed bonuses into unique_* layer (Phase 4.2). */
static void gather_unique_primary(void)
{
    int str = 0, dex = 0, vit = 0, intel = 0;
    int armor_flat = 0;
    int r_phys = 0, r_fire = 0, r_cold = 0, r_light = 0, r_poison = 0, r_status = 0;
    for (int slot = 0; slot < ROGUE_EQUIP__COUNT; ++slot)
    {
        int inst_index = rogue_equip_get((enum RogueEquipSlot) slot);
        if (inst_index < 0)
            continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst_index);
        if (!it)
            continue;
        const RogueItemDef* d = rogue_item_def_at(it->def_index);
        if (!d)
            continue;
        int uidx = rogue_unique_find_by_base_def(it->def_index);
        if (uidx < 0)
            continue;
        const RogueUniqueDef* u = rogue_unique_at(uidx);
        if (!u)
            continue;
        str += u->strength;
        dex += u->dexterity;
        vit += u->vitality;
        intel += u->intelligence;
        armor_flat += u->armor_flat;
        r_phys += u->resist_physical;
        r_fire += u->resist_fire;
        r_cold += u->resist_cold;
        r_light += u->resist_lightning;
        r_poison += u->resist_poison;
        r_status += u->resist_status;
    }
    g_player_stat_cache.unique_strength = str;
    g_player_stat_cache.unique_dexterity = dex;
    g_player_stat_cache.unique_vitality = vit;
    g_player_stat_cache.unique_intelligence = intel;
    g_player_stat_cache.affix_armor_flat +=
        armor_flat; /* fold into shared flat armor contribution */
    g_player_stat_cache.resist_physical += r_phys;
    g_player_stat_cache.resist_fire += r_fire;
    g_player_stat_cache.resist_cold += r_cold;
    g_player_stat_cache.resist_lightning += r_light;
    g_player_stat_cache.resist_poison += r_poison;
    g_player_stat_cache.resist_status += r_status;
}

/* Aggregate set bonuses (Phase 4.3 & 4.4 partial scaling). Approach: count equipped items per
 * set_id, then apply any bonuses whose threshold <= count. Partial scaling: if count is between
 * thresholds and next threshold exists, interpolate linearly. */
static void gather_set_bonuses(void)
{
    int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
    int r_phys = 0, r_fire = 0, r_cold = 0, r_light = 0, r_poison = 0, r_status = 0;
    /* For each distinct set encountered gather count first (small N approach). */
    for (int si = 0; si < rogue_set_count(); ++si)
    {
        const RogueSetDef* sd = rogue_set_at(si);
        if (!sd)
            continue;
        int have = 0;
        for (int slot = 0; slot < ROGUE_EQUIP__COUNT; ++slot)
        {
            int inst = rogue_equip_get((enum RogueEquipSlot) slot);
            if (inst < 0)
                continue;
            const RogueItemInstance* it = rogue_item_instance_at(inst);
            if (!it)
                continue;
            const RogueItemDef* d = rogue_item_def_at(it->def_index);
            if (!d)
                continue;
            if (d->set_id == sd->set_id)
                have++;
        }
        if (have > 0)
        {
            rogue_set_preview_apply(sd->set_id, have, &str, &dex, &vit, &intel, &armor, &r_fire,
                                    &r_cold, &r_light, &r_poison, &r_status, &r_phys);
        }
    }
    g_player_stat_cache.set_strength = str;
    g_player_stat_cache.set_dexterity = dex;
    g_player_stat_cache.set_vitality = vit;
    g_player_stat_cache.set_intelligence = intel;
    g_player_stat_cache.affix_armor_flat += armor;
    g_player_stat_cache.resist_physical += r_phys;
    g_player_stat_cache.resist_fire += r_fire;
    g_player_stat_cache.resist_cold += r_cold;
    g_player_stat_cache.resist_lightning += r_light;
    g_player_stat_cache.resist_poison += r_poison;
    g_player_stat_cache.resist_status += r_status;
}

/* Aggregate runeword bonuses (Phase 4.5) */
static void gather_runeword_bonuses(void)
{
    int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
    int r_phys = 0, r_fire = 0, r_cold = 0, r_light = 0, r_poison = 0, r_status = 0;
    for (int slot = 0; slot < ROGUE_EQUIP__COUNT; ++slot)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) slot);
        if (inst < 0)
            continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst);
        if (!it)
            continue;
        const RogueItemDef* d = rogue_item_def_at(it->def_index);
        if (!d)
            continue;
        const RogueRuneword* rw = item_runeword(d);
        if (!rw)
            continue;
        str += rw->strength;
        dex += rw->dexterity;
        vit += rw->vitality;
        intel += rw->intelligence;
        armor += rw->armor_flat;
        r_phys += rw->resist_physical;
        r_fire += rw->resist_fire;
        r_cold += rw->resist_cold;
        r_light += rw->resist_light;
        r_poison += rw->resist_poison;
        r_status += rw->resist_status;
    }
    g_player_stat_cache.runeword_strength = str;
    g_player_stat_cache.runeword_dexterity = dex;
    g_player_stat_cache.runeword_vitality = vit;
    g_player_stat_cache.runeword_intelligence = intel;
    g_player_stat_cache.affix_armor_flat += armor;
    g_player_stat_cache.resist_physical += r_phys;
    g_player_stat_cache.resist_fire += r_fire;
    g_player_stat_cache.resist_cold += r_cold;
    g_player_stat_cache.resist_lightning += r_light;
    g_player_stat_cache.resist_poison += r_poison;
    g_player_stat_cache.resist_status += r_status;
}

void rogue_equipment_apply_stat_bonuses(RoguePlayer* p)
{
    /* Maintain layered cache model but also (legacy test compatibility) reflect primary stat deltas
     * into player struct if provided. */
    /* Reset dynamic aggregation fields we own before recomputing. Base & implicit fields cleared in
     * stat cache compute_layers. */
    g_player_stat_cache.affix_strength = g_player_stat_cache.affix_dexterity = 0;
    g_player_stat_cache.affix_vitality = g_player_stat_cache.affix_intelligence = 0;
    g_player_stat_cache.affix_armor_flat = 0;
    /* Resist fields reset to 0 before summing from affix + implicit layers. */
    g_player_stat_cache.resist_physical = g_player_stat_cache.resist_fire = 0;
    g_player_stat_cache.resist_cold = g_player_stat_cache.resist_lightning = 0;
    g_player_stat_cache.resist_poison = g_player_stat_cache.resist_status = 0;
    g_player_stat_cache.block_chance = g_player_stat_cache.block_value = 0;
    g_player_stat_cache.phys_conv_fire_pct = g_player_stat_cache.phys_conv_frost_pct = 0;
    g_player_stat_cache.phys_conv_arcane_pct = 0;
    g_player_stat_cache.guard_recovery_pct = 0;
    g_player_stat_cache.thorns_percent = 0;
    g_player_stat_cache.thorns_cap = 0;
    gather_affix_primary_and_armor(); /* affix layer (later in precedence) */
    gather_implicit_primary_and_armor();
    gather_unique_primary();
    rogue_gems_aggregate_equipped(); /* Phase 5.2: gem contributions before sets/runewords (still
                                        additive into affix_* layer) */
    gather_set_bonuses();
    gather_runeword_bonuses();
    rogue_stat_cache_mark_dirty();
    if (p)
    {
        /* Idempotent application: track previously applied equipment deltas so repeated calls
           do not compound base stats and drift the fingerprint
           (test_equipment_phase18_stress_combo). We compute against a reconstructed baseline each
           invocation. */
        static int prev_applied_str = 0, prev_applied_dex = 0, prev_applied_vit = 0,
                   prev_applied_int = 0;
        int base_str = p->strength - prev_applied_str;
        if (base_str < 0)
            base_str = 0;
        int base_dex = p->dexterity - prev_applied_dex;
        if (base_dex < 0)
            base_dex = 0;
        int base_vit = p->vitality - prev_applied_vit;
        if (base_vit < 0)
            base_vit = 0;
        int base_int = p->intelligence - prev_applied_int;
        if (base_int < 0)
            base_int = 0;
        /* Use a temp copy so stat cache sees original base values, not mutated player fields */
        RoguePlayer temp = *p;
        temp.strength = base_str;
        temp.dexterity = base_dex;
        temp.vitality = base_vit;
        temp.intelligence = base_int;
        rogue_stat_cache_force_update(&temp); /* recompute layers on baseline */
        /* Apply fresh deltas to player (visible legacy behavior) */
        int applied_str = g_player_stat_cache.total_strength - g_player_stat_cache.base_strength;
        if (applied_str < 0)
            applied_str = 0;
        int applied_dex = g_player_stat_cache.total_dexterity - g_player_stat_cache.base_dexterity;
        if (applied_dex < 0)
            applied_dex = 0;
        int applied_vit = g_player_stat_cache.total_vitality - g_player_stat_cache.base_vitality;
        if (applied_vit < 0)
            applied_vit = 0;
        int applied_int =
            g_player_stat_cache.total_intelligence - g_player_stat_cache.base_intelligence;
        if (applied_int < 0)
            applied_int = 0;
        p->strength = base_str + applied_str;
        p->dexterity = base_dex + applied_dex;
        p->vitality = base_vit + applied_vit;
        p->intelligence = base_int + applied_int;
        /* Persist for next invocation */
        prev_applied_str = applied_str;
        prev_applied_dex = applied_dex;
        prev_applied_vit = applied_vit;
        prev_applied_int = applied_int;
        /* Cache already up-to-date from temp path; no further force_update needed. */
    }
    else
    {
        /* No player pointer: just finalize cache */
        extern RoguePlayer g_exposed_player_for_stats; /* fallback safe player for recompute */
        rogue_stat_cache_force_update(&g_exposed_player_for_stats);
    }
}
