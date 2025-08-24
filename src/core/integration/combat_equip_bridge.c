#include "combat_equip_bridge.h"
#include "../../entities/player.h"
#include "../../game/combat.h"
#include "../../util/log.h"
#include "../equipment/equipment.h"
#include "../equipment/equipment_procs.h"
#include "../equipment/equipment_stats.h"
#include "event_bus.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static double get_time_microseconds()
{
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000.0) / freq.QuadPart;
}
#else
#include <sys/time.h>
static double get_time_microseconds()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}
#endif

/* Debug logging macro */
#define BRIDGE_LOG(bridge, level, fmt, ...)                                                        \
    do                                                                                             \
    {                                                                                              \
        if ((bridge)->debug_logging)                                                               \
        {                                                                                          \
            printf("[Combat-Equipment Bridge %s] " fmt "\n", level, ##__VA_ARGS__);                \
        }                                                                                          \
    } while (0)

/* Phase 3.2.1: Real-time equipment stat application to combat calculations */

int rogue_combat_equip_bridge_init(RogueCombatEquipBridge* bridge)
{
    if (!bridge)
    {
        return 0;
    }

    memset(bridge, 0, sizeof(RogueCombatEquipBridge));

    /* Initialize performance settings */
    bridge->stat_update_interval_ms = 16.67f;    /* ~60 FPS stat updates */
    bridge->weight_update_interval_ms = 50.0f;   /* 20 FPS weight updates */
    bridge->max_durability_events_per_frame = 8; /* Max 8 durability events per frame */
    bridge->max_proc_activations_per_frame = 12; /* Max 12 proc activations per frame */

    /* Initialize cache state */
    bridge->stats_dirty = true;
    bridge->weight_dirty = true;
    bridge->last_equipment_change_timestamp = 0;

    /* Initialize performance metrics */
    bridge->metrics.last_metrics_reset = time(NULL);

    bridge->initialized = true;

    BRIDGE_LOG(bridge, "INFO",
               "Combat-Equipment Bridge initialized successfully with real-time stat integration");
    return 1;
}

void rogue_combat_equip_bridge_shutdown(RogueCombatEquipBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return;
    }

    BRIDGE_LOG(bridge, "INFO", "Combat-Equipment Bridge shutdown complete");
    bridge->initialized = false;
}

int rogue_combat_equip_bridge_update_stats(RogueCombatEquipBridge* bridge,
                                           struct RoguePlayer* player)
{
    if (!bridge || !bridge->initialized || !player)
    {
        return 0;
    }

    if (!bridge->stats_dirty)
    {
        return 1; /* Stats are current */
    }

    double start_time = get_time_microseconds();

    /* Reset combat equipment stats */
    memset(&bridge->cached_stats, 0, sizeof(RogueCombatEquipmentStats));

    /* Base multipliers */
    bridge->cached_stats.damage_multiplier = 1.0f;
    bridge->cached_stats.attack_speed_multiplier = 1.0f;
    bridge->cached_stats.crit_damage_multiplier = 1.0f;

    /* Iterate through all equipped items and calculate stat bonuses */
    for (int slot = 0; slot < ROGUE_EQUIP_SLOT_COUNT; slot++)
    {
        int item_instance = rogue_equip_get(slot);
        if (item_instance < 0)
            continue; /* No item equipped in this slot */

        /* TODO: Get item stats from equipment system and apply to cached_stats */
        /* This would integrate with the actual equipment stat system */
        /* For now, apply placeholder bonuses based on slot */

        if (slot == ROGUE_EQUIP_WEAPON)
        {
            bridge->cached_stats.damage_multiplier += 0.15f; /* +15% weapon damage */
            bridge->cached_stats.crit_chance_bonus += 5.0f;  /* +5% crit chance */
        }
        else if (slot >= ROGUE_EQUIP_ARMOR_HEAD && slot <= ROGUE_EQUIP_ARMOR_FEET)
        {
            /* Armor pieces provide defensive bonuses */
            bridge->cached_stats.elemental_damage |= (1 << slot); /* Placeholder elemental resist */
        }
        else if (slot == ROGUE_EQUIP_RING1 || slot == ROGUE_EQUIP_RING2)
        {
            bridge->cached_stats.crit_damage_multiplier += 0.08f; /* +8% crit damage per ring */
        }
    }

    bridge->stats_dirty = false;

    double end_time = get_time_microseconds();
    double calc_time = end_time - start_time;

    /* Update performance metrics */
    bridge->metrics.stat_calculations_per_second++;
    bridge->metrics.average_stat_calc_time_us =
        (bridge->metrics.average_stat_calc_time_us * 0.95f) + ((float) calc_time * 0.05f);
    if ((float) calc_time > bridge->metrics.peak_stat_calc_time_us)
    {
        bridge->metrics.peak_stat_calc_time_us = (float) calc_time;
    }

    BRIDGE_LOG(bridge, "INFO",
               "Equipment stats updated: dmg_mult=%.2f, crit_chance=+%.1f%%, calc_time=%.2f us",
               bridge->cached_stats.damage_multiplier, bridge->cached_stats.crit_chance_bonus,
               calc_time);

    return 1;
}

int rogue_combat_equip_bridge_get_combat_stats(RogueCombatEquipBridge* bridge,
                                               RogueCombatEquipmentStats* out_stats)
{
    if (!bridge || !bridge->initialized || !out_stats)
    {
        return 0;
    }

    *out_stats = bridge->cached_stats;
    bridge->metrics.cache_hits++;
    return 1;
}

int rogue_combat_equip_bridge_apply_stats_to_combat(RogueCombatEquipBridge* bridge,
                                                    struct RoguePlayerCombat* combat)
{
    if (!bridge || !bridge->initialized || !combat)
    {
        return 0;
    }

    /* Apply cached equipment stats to combat calculations */
    /* This would integrate with the actual combat system to modify damage, timing, etc. */

    BRIDGE_LOG(
        bridge, "INFO", "Applied equipment stats to combat: dmg_mult=%.2f, attack_speed_mult=%.2f",
        bridge->cached_stats.damage_multiplier, bridge->cached_stats.attack_speed_multiplier);

    return 1;
}

/* Phase 3.2.2: Equipment durability reduction hooks in combat damage events */

int rogue_combat_equip_bridge_on_damage_taken(RogueCombatEquipBridge* bridge,
                                              struct RoguePlayer* player, uint32_t damage_amount,
                                              uint8_t damage_type)
{
    if (!bridge || !bridge->initialized || !player)
    {
        return 0;
    }

    /* Suppress unused parameter warning */
    (void) damage_type;

    if (bridge->durability_event_count >= bridge->max_durability_events_per_frame)
    {
        BRIDGE_LOG(bridge, "WARN", "Durability event limit reached (%u), dropping event",
                   bridge->max_durability_events_per_frame);
        return 0;
    }

    /* Calculate durability damage based on incoming damage */
    uint16_t durability_damage =
        (uint16_t) ((damage_amount / 20) + 1); /* 1 durability per 20 damage + 1 base */

    /* Apply durability damage to armor pieces */
    uint8_t armor_slots[] = {ROGUE_EQUIP_ARMOR_CHEST, ROGUE_EQUIP_ARMOR_HEAD,
                             ROGUE_EQUIP_ARMOR_LEGS, ROGUE_EQUIP_ARMOR_HANDS,
                             ROGUE_EQUIP_ARMOR_FEET};

    for (int i = 0; i < 5; i++)
    {
        uint8_t slot = armor_slots[i];
        int item_instance = rogue_equip_get(slot);
        if (item_instance < 0)
            continue;

        /* Create durability event */
        RogueEquipmentDurabilityEvent* event =
            &bridge->durability_events[bridge->durability_event_count];
        event->slot = slot;
        event->damage_taken = durability_damage;
        event->remaining_durability = 100; /* TODO: Get actual durability from equipment system */
        event->broken = event->remaining_durability <= durability_damage;
        event->combat_event_id = bridge->metrics.durability_events_processed + 1;

        bridge->durability_event_count++;

        BRIDGE_LOG(bridge, "INFO", "Durability damage to slot %u: -%u points, %u remaining", slot,
                   durability_damage, event->remaining_durability);

        if (event->broken)
        {
            BRIDGE_LOG(bridge, "WARN", "Equipment in slot %u BROKEN during combat!", slot);
            /* TODO: Trigger equipment broken event */
        }
    }

    return 1;
}

int rogue_combat_equip_bridge_on_attack_made(RogueCombatEquipBridge* bridge,
                                             struct RoguePlayer* player, bool hit_target)
{
    if (!bridge || !bridge->initialized || !player)
    {
        return 0;
    }

    /* Weapon durability damage on attack */
    int weapon_instance = rogue_equip_get(ROGUE_EQUIP_WEAPON);
    if (weapon_instance >= 0)
    {
        uint16_t weapon_durability_damage = hit_target ? 2 : 1; /* More damage if hit target */

        RogueEquipmentDurabilityEvent* event =
            &bridge->durability_events[bridge->durability_event_count];
        event->slot = ROGUE_EQUIP_WEAPON;
        event->damage_taken = weapon_durability_damage;
        event->remaining_durability = 100; /* TODO: Get actual durability */
        event->broken = event->remaining_durability <= weapon_durability_damage;
        event->combat_event_id = bridge->metrics.durability_events_processed + 1;

        /* Use the event to avoid warning */
        (void) event;

        bridge->durability_event_count++;

        BRIDGE_LOG(bridge, "INFO", "Weapon durability damage: -%u points (%s)",
                   weapon_durability_damage, hit_target ? "hit" : "miss");
    }

    return 1;
}

int rogue_combat_equip_bridge_process_durability_events(RogueCombatEquipBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    uint8_t processed_events = bridge->durability_event_count;

    /* Process all pending durability events */
    for (uint8_t i = 0; i < bridge->durability_event_count; i++)
    {
        RogueEquipmentDurabilityEvent* event = &bridge->durability_events[i];

        /* TODO: Apply durability damage to actual equipment system */
        /* This would call into equipment system to reduce item durability */

        /* Use the event to avoid warning */
        (void) event;

        bridge->metrics.durability_events_processed++;
    }

    /* Clear processed events */
    bridge->durability_event_count = 0;

    if (processed_events > 0)
    {
        BRIDGE_LOG(bridge, "INFO", "Processed %u durability events", processed_events);
    }

    return processed_events;
}

/* Phase 3.2.3: Equipment proc effect triggers during combat actions */

int rogue_combat_equip_bridge_trigger_procs(RogueCombatEquipBridge* bridge, uint8_t trigger_type,
                                            uint32_t context_data)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    if (bridge->active_proc_count >= bridge->max_proc_activations_per_frame)
    {
        BRIDGE_LOG(bridge, "WARN", "Proc activation limit reached (%u), dropping activation",
                   bridge->max_proc_activations_per_frame);
        return 0;
    }

    /* TODO: Integrate with actual equipment proc system */
    /* For now, simulate proc activation */

    /* Check all equipped items for procs that match this trigger */
    for (int slot = 0; slot < ROGUE_EQUIP_SLOT_COUNT; slot++)
    {
        int item_instance = rogue_equip_get(slot);
        if (item_instance < 0)
            continue;

        /* Simulate 15% chance for any item to have a proc that triggers */
        if ((context_data + slot) % 100 < 15)
        {
            RogueEquipmentProcActivation* proc = &bridge->active_procs[bridge->active_proc_count];
            proc->proc_id = (uint16_t) (1000 + slot); /* Simulated proc ID */
            proc->trigger_type = trigger_type;
            proc->stacks_applied = 1;
            proc->duration_remaining_ms = 5000; /* 5 second duration */
            proc->magnitude = 25 + (slot * 5);  /* Variable magnitude */
            proc->combat_context_id = context_data;

            /* Use the proc to avoid warning */
            (void) proc;

            bridge->active_proc_count++;
            bridge->metrics.procs_triggered_total++;

            BRIDGE_LOG(bridge, "INFO", "Proc %u triggered from slot %d: type=%u, magnitude=%d",
                       proc->proc_id, slot, trigger_type, proc->magnitude);
        }
    }

    return 1;
}

int rogue_combat_equip_bridge_update_active_procs(RogueCombatEquipBridge* bridge, float dt_ms)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    /* Update proc durations and remove expired procs */
    uint8_t active_count = 0;
    for (uint8_t i = 0; i < bridge->active_proc_count; i++)
    {
        RogueEquipmentProcActivation* proc = &bridge->active_procs[i];

        proc->duration_remaining_ms -= (uint16_t) dt_ms;

        if (proc->duration_remaining_ms > 0)
        {
            /* Keep active proc, move it to compact position */
            if (active_count != i)
            {
                bridge->active_procs[active_count] = *proc;
            }
            active_count++;
        }
        else
        {
            BRIDGE_LOG(bridge, "INFO", "Proc %u expired", proc->proc_id);
        }
    }

    bridge->active_proc_count = active_count;
    return active_count;
}

int rogue_combat_equip_bridge_get_active_procs(RogueCombatEquipBridge* bridge,
                                               RogueEquipmentProcActivation* out_procs,
                                               uint8_t max_procs)
{
    if (!bridge || !bridge->initialized || !out_procs)
    {
        return 0;
    }

    uint8_t copy_count = bridge->active_proc_count;
    if (copy_count > max_procs)
    {
        copy_count = max_procs;
    }

    memcpy(out_procs, bridge->active_procs, copy_count * sizeof(RogueEquipmentProcActivation));
    return copy_count;
}

/* Phase 3.2.4: Equipment set bonus activation/deactivation on equip/unequip */

int rogue_combat_equip_bridge_update_set_bonuses(RogueCombatEquipBridge* bridge,
                                                 struct RoguePlayer* player)
{
    if (!bridge || !bridge->initialized || !player)
    {
        return 0;
    }

    /* Clear previous set bonus states */
    for (uint8_t i = 0; i < bridge->set_bonus_count; i++)
    {
        bridge->set_bonuses[i].just_activated = false;
        bridge->set_bonuses[i].just_deactivated = false;
    }

    /* Scan equipped items and count set pieces */
    /* TODO: Integrate with actual equipment set system */
    /* For now, simulate set detection */

    /* Simulate "Warrior Set" detection */
    uint8_t warrior_pieces = 0;
    uint8_t armor_slots[] = {ROGUE_EQUIP_ARMOR_CHEST, ROGUE_EQUIP_ARMOR_HEAD,
                             ROGUE_EQUIP_ARMOR_LEGS, ROGUE_EQUIP_ARMOR_HANDS};
    for (int i = 0; i < 4; i++)
    {
        if (rogue_equip_get(armor_slots[i]) >= 0)
        {
            warrior_pieces++; /* Assume equipped armor is part of warrior set */
        }
    }

    /* Update set bonus state */
    RogueEquipmentSetBonusState* warrior_set = &bridge->set_bonuses[0];
    uint8_t old_pieces = warrior_set->pieces_equipped;
    uint8_t old_tier = warrior_set->bonus_tier;

    /* Use old values to avoid warnings */
    (void) old_pieces;
    (void) old_tier;

    warrior_set->set_id = 1001; /* Warrior set ID */
    warrior_set->pieces_equipped = warrior_pieces;
    warrior_set->bonus_tier = (warrior_pieces >= 4) ? 2 : (warrior_pieces >= 2) ? 1 : 0;
    warrior_set->bonus_flags = (warrior_set->bonus_tier >= 1) ? 0x01 : 0; /* Tier 1 bonus */
    if (warrior_set->bonus_tier >= 2)
    {
        warrior_set->bonus_flags |= 0x02; /* Tier 2 bonus */
    }

    /* Detect state changes */
    if (warrior_set->bonus_tier > old_tier)
    {
        warrior_set->just_activated = true;
        bridge->metrics.set_bonus_state_changes++;
        BRIDGE_LOG(bridge, "INFO", "Warrior Set bonus tier %u ACTIVATED (%u pieces equipped)",
                   warrior_set->bonus_tier, warrior_pieces);
    }
    else if (warrior_set->bonus_tier < old_tier)
    {
        warrior_set->just_deactivated = true;
        bridge->metrics.set_bonus_state_changes++;
        BRIDGE_LOG(bridge, "INFO", "Warrior Set bonus tier %u DEACTIVATED (%u pieces equipped)",
                   old_tier, warrior_pieces);
    }

    bridge->set_bonus_count = 1; /* Currently tracking 1 set */

    return bridge->set_bonus_count;
}

int rogue_combat_equip_bridge_get_set_bonuses(RogueCombatEquipBridge* bridge,
                                              RogueEquipmentSetBonusState* out_bonuses,
                                              uint8_t max_bonuses)
{
    if (!bridge || !bridge->initialized || !out_bonuses)
    {
        return 0;
    }

    uint8_t copy_count = bridge->set_bonus_count;
    if (copy_count > max_bonuses)
    {
        copy_count = max_bonuses;
    }

    memcpy(out_bonuses, bridge->set_bonuses, copy_count * sizeof(RogueEquipmentSetBonusState));
    return copy_count;
}

int rogue_combat_equip_bridge_apply_set_bonuses_to_combat(RogueCombatEquipBridge* bridge,
                                                          struct RoguePlayerCombat* combat)
{
    if (!bridge || !bridge->initialized || !combat)
    {
        return 0;
    }

    /* Apply active set bonuses to combat calculations */
    for (uint8_t i = 0; i < bridge->set_bonus_count; i++)
    {
        RogueEquipmentSetBonusState* set = &bridge->set_bonuses[i];
        if (set->bonus_tier == 0)
            continue;

        /* Apply set bonuses to combat stats */
        /* TODO: Integrate with actual combat system */

        BRIDGE_LOG(bridge, "INFO", "Applied set %u tier %u bonuses to combat (flags: 0x%X)",
                   set->set_id, set->bonus_tier, set->bonus_flags);
    }

    return 1;
}

/* Phase 3.2.5: Equipment enchantment effects integration in combat formulas */

int rogue_combat_equip_bridge_apply_enchantments(RogueCombatEquipBridge* bridge,
                                                 struct RoguePlayer* player,
                                                 float* damage_multiplier,
                                                 uint32_t* elemental_damage)
{
    if (!bridge || !bridge->initialized || !player || !damage_multiplier || !elemental_damage)
    {
        return 0;
    }

    /* Scan equipped items for enchantments */
    for (int slot = 0; slot < ROGUE_EQUIP_SLOT_COUNT; slot++)
    {
        int item_instance = rogue_equip_get(slot);
        if (item_instance < 0)
            continue;

        /* TODO: Get enchantments from actual equipment system */
        /* For now, simulate enchantment effects */

        if (slot == ROGUE_EQUIP_WEAPON)
        {
            *damage_multiplier *= 1.12f;     /* +12% damage enchantment */
            *elemental_damage |= 0x00FF0000; /* Fire damage enchantment */
        }
        else if (slot >= ROGUE_EQUIP_RING1 && slot <= ROGUE_EQUIP_RING2)
        {
            *elemental_damage |= 0x0000FF00; /* Lightning damage enchantment */
        }
    }

    bridge->metrics.enchantment_applications++;

    BRIDGE_LOG(bridge, "INFO", "Applied enchantments: dmg_mult=%.2f, elemental=0x%08X",
               *damage_multiplier, *elemental_damage);

    return 1;
}

int rogue_combat_equip_bridge_trigger_enchantment_effects(RogueCombatEquipBridge* bridge,
                                                          uint8_t enchant_trigger,
                                                          uint32_t context_data)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    /* TODO: Implement enchantment effect triggering */
    /* This would integrate with enchantment system to trigger on-hit, on-crit effects, etc. */

    BRIDGE_LOG(bridge, "INFO", "Enchantment effects triggered: type=%u, context=0x%08X",
               enchant_trigger, context_data);

    return 1;
}

/* Phase 3.2.6: Equipment weight impact on combat timing & movement */

int rogue_combat_equip_bridge_update_weight_impact(RogueCombatEquipBridge* bridge,
                                                   struct RoguePlayer* player)
{
    if (!bridge || !bridge->initialized || !player)
    {
        return 0;
    }

    if (!bridge->weight_dirty)
    {
        return 1; /* Weight is current */
    }

    /* Calculate total equipped weight */
    float total_weight = 0.0f;
    for (int slot = 0; slot < ROGUE_EQUIP_SLOT_COUNT; slot++)
    {
        int item_instance = rogue_equip_get(slot);
        if (item_instance < 0)
            continue;

        /* TODO: Get actual item weight from equipment system */
        /* For now, simulate weights */
        if (slot == ROGUE_EQUIP_WEAPON)
        {
            total_weight += 3.5f; /* Weapon weight */
        }
        else if (slot >= ROGUE_EQUIP_ARMOR_HEAD && slot <= ROGUE_EQUIP_ARMOR_FEET)
        {
            total_weight += 2.0f; /* Armor piece weight */
        }
        else
        {
            total_weight += 0.5f; /* Accessory weight */
        }
    }

    /* Calculate weight impact */
    float weight_limit = 25.0f; /* Player weight limit */
    bridge->weight_impact.total_weight = total_weight;
    bridge->weight_impact.encumbered = total_weight > weight_limit;

    if (bridge->weight_impact.encumbered)
    {
        float over_weight = total_weight - weight_limit;
        bridge->weight_impact.weight_penalty = over_weight / weight_limit; /* Penalty ratio */
        bridge->weight_impact.attack_speed_modifier =
            1.0f - (bridge->weight_impact.weight_penalty * 0.3f);
        bridge->weight_impact.dodge_speed_modifier =
            1.0f - (bridge->weight_impact.weight_penalty * 0.5f);
        bridge->weight_impact.stamina_drain_multiplier =
            1.0f + (bridge->weight_impact.weight_penalty * 0.8f);
    }
    else
    {
        bridge->weight_impact.weight_penalty = 0.0f;
        bridge->weight_impact.attack_speed_modifier = 1.0f;
        bridge->weight_impact.dodge_speed_modifier = 1.0f;
        bridge->weight_impact.stamina_drain_multiplier = 1.0f;
    }

    bridge->weight_dirty = false;
    bridge->metrics.weight_calculations_per_second++;

    BRIDGE_LOG(bridge, "INFO",
               "Weight impact updated: total=%.1f, encumbered=%s, attack_speed=%.2f", total_weight,
               bridge->weight_impact.encumbered ? "YES" : "NO",
               bridge->weight_impact.attack_speed_modifier);

    return 1;
}

int rogue_combat_equip_bridge_get_weight_impact(RogueCombatEquipBridge* bridge,
                                                RogueEquipmentWeightImpact* out_impact)
{
    if (!bridge || !bridge->initialized || !out_impact)
    {
        return 0;
    }

    *out_impact = bridge->weight_impact;
    return 1;
}

int rogue_combat_equip_bridge_apply_weight_to_combat(RogueCombatEquipBridge* bridge,
                                                     struct RoguePlayerCombat* combat)
{
    if (!bridge || !bridge->initialized || !combat)
    {
        return 0;
    }

    /* Apply weight impact to combat timing and movement */
    /* TODO: Integrate with actual combat system to modify attack timing, dodge speed, etc. */

    BRIDGE_LOG(
        bridge, "INFO",
        "Applied weight impact to combat: attack_speed=%.2f, dodge_speed=%.2f, stamina_mult=%.2f",
        bridge->weight_impact.attack_speed_modifier, bridge->weight_impact.dodge_speed_modifier,
        bridge->weight_impact.stamina_drain_multiplier);

    return 1;
}

/* Phase 3.2.7: Equipment upgrade notifications to combat stat cache */

int rogue_combat_equip_bridge_on_equipment_upgraded(RogueCombatEquipBridge* bridge, uint8_t slot,
                                                    uint32_t old_item_id, uint32_t new_item_id)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    /* Mark stats as dirty for recalculation */
    bridge->stats_dirty = true;
    bridge->weight_dirty = true;
    bridge->last_equipment_change_timestamp = (uint64_t) time(NULL);

    BRIDGE_LOG(bridge, "INFO",
               "Equipment upgraded in slot %u: %u -> %u, invalidating combat stat cache", slot,
               old_item_id, new_item_id);

    /* Publish equipment change event */
    RogueEventPayload payload = {0};
    memcpy(payload.raw_data, &slot, sizeof(uint8_t));
    rogue_event_publish(ROGUE_COMBAT_EQUIP_EVENT_UPGRADE_NOTIFICATION, &payload,
                        ROGUE_EVENT_PRIORITY_NORMAL, 0, "CombatEquipBridge");

    return 1;
}

int rogue_combat_equip_bridge_on_equipment_enchanted(RogueCombatEquipBridge* bridge, uint8_t slot,
                                                     uint32_t enchant_id)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    bridge->stats_dirty = true;
    bridge->last_equipment_change_timestamp = (uint64_t) time(NULL);

    BRIDGE_LOG(bridge, "INFO",
               "Equipment enchanted in slot %u with enchant %u, invalidating combat stat cache",
               slot, enchant_id);

    RogueEventPayload payload = {0};
    memcpy(payload.raw_data, &enchant_id, sizeof(uint32_t));
    rogue_event_publish(ROGUE_COMBAT_EQUIP_EVENT_ENCHANT_APPLIED, &payload,
                        ROGUE_EVENT_PRIORITY_NORMAL, 0, "CombatEquipBridge");

    return 1;
}

int rogue_combat_equip_bridge_on_equipment_socketed(RogueCombatEquipBridge* bridge, uint8_t slot,
                                                    uint32_t gem_id)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    bridge->stats_dirty = true;
    bridge->last_equipment_change_timestamp = (uint64_t) time(NULL);

    BRIDGE_LOG(bridge, "INFO",
               "Equipment socketed in slot %u with gem %u, invalidating combat stat cache", slot,
               gem_id);

    return 1;
}

/* Performance & Debug Functions */

int rogue_combat_equip_bridge_get_metrics(RogueCombatEquipBridge* bridge,
                                          RogueCombatEquipBridgeMetrics* out_metrics)
{
    if (!bridge || !bridge->initialized || !out_metrics)
    {
        return 0;
    }

    *out_metrics = bridge->metrics;
    return 1;
}

void rogue_combat_equip_bridge_reset_metrics(RogueCombatEquipBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return;
    }

    memset(&bridge->metrics, 0, sizeof(RogueCombatEquipBridgeMetrics));
    bridge->metrics.last_metrics_reset = time(NULL);

    BRIDGE_LOG(bridge, "INFO", "Performance metrics reset");
}

int rogue_combat_equip_bridge_check_performance_thresholds(RogueCombatEquipBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return 0;
    }

    int warnings = 0;

    /* Check performance thresholds */
    if (bridge->metrics.peak_stat_calc_time_us > 1000.0f)
    { /* 1ms threshold */
        BRIDGE_LOG(bridge, "WARN", "Stat calculation time exceeded threshold: %.2f us",
                   bridge->metrics.peak_stat_calc_time_us);
        warnings++;
    }

    if (bridge->metrics.cache_misses > bridge->metrics.cache_hits * 0.2f)
    { /* 20% miss rate threshold */
        BRIDGE_LOG(bridge, "WARN", "Cache miss rate high: %u misses vs %u hits",
                   bridge->metrics.cache_misses, bridge->metrics.cache_hits);
        warnings++;
    }

    return warnings;
}

void rogue_combat_equip_bridge_set_debug_logging(RogueCombatEquipBridge* bridge, bool enabled)
{
    if (!bridge)
        return;
    bridge->debug_logging = enabled;
    BRIDGE_LOG(bridge, "INFO", "Debug logging %s", enabled ? "ENABLED" : "DISABLED");
}

int rogue_combat_equip_bridge_get_debug_status(RogueCombatEquipBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        return -1;
    }
    return bridge->debug_logging ? 1 : 0;
}

int rogue_combat_equip_bridge_validate(RogueCombatEquipBridge* bridge)
{
    if (!bridge)
    {
        printf("[Combat-Equipment Bridge Validate] ERROR: Bridge pointer is NULL\n");
        return 0;
    }

    if (!bridge->initialized)
    {
        printf("[Combat-Equipment Bridge Validate] ERROR: Bridge not initialized\n");
        return 0;
    }

    /* Validate internal state consistency */
    if (bridge->durability_event_count > 64)
    {
        printf("[Combat-Equipment Bridge Validate] ERROR: Invalid durability event count: %u\n",
               bridge->durability_event_count);
        return 0;
    }

    if (bridge->active_proc_count > 32)
    {
        printf("[Combat-Equipment Bridge Validate] ERROR: Invalid active proc count: %u\n",
               bridge->active_proc_count);
        return 0;
    }

    if (bridge->set_bonus_count > 16)
    {
        printf("[Combat-Equipment Bridge Validate] ERROR: Invalid set bonus count: %u\n",
               bridge->set_bonus_count);
        return 0;
    }

    printf("[Combat-Equipment Bridge Validate] SUCCESS: Bridge state is valid\n");
    return 1;
}
