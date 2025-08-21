#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Forward declarations */
    struct RoguePlayer;
    struct RogueEnemy;
    struct RoguePlayerCombat;

    /* Phase 3.2 Combat System ↔ Equipment System Bridge */

    /* Combat-Equipment Event Types */
    typedef enum
    {
        ROGUE_COMBAT_EQUIP_EVENT_STAT_UPDATE = 3200,  /* Equipment stats changed */
        ROGUE_COMBAT_EQUIP_EVENT_DURABILITY_DAMAGE,   /* Combat damaged equipment */
        ROGUE_COMBAT_EQUIP_EVENT_PROC_TRIGGERED,      /* Equipment proc activated */
        ROGUE_COMBAT_EQUIP_EVENT_SET_BONUS_CHANGED,   /* Set bonus activated/deactivated */
        ROGUE_COMBAT_EQUIP_EVENT_ENCHANT_APPLIED,     /* Enchantment effect triggered */
        ROGUE_COMBAT_EQUIP_EVENT_WEIGHT_CHANGED,      /* Equipment weight modified combat */
        ROGUE_COMBAT_EQUIP_EVENT_UPGRADE_NOTIFICATION /* Equipment upgraded, stats changed */
    } RogueCombatEquipEventType;

    /* Equipment impact on combat calculations */
    typedef struct
    {
        float damage_multiplier;       /* From weapon/enchants */
        float attack_speed_multiplier; /* From weapon weight/enchants */
        float crit_chance_bonus;       /* From equipment affixes */
        float crit_damage_multiplier;  /* From equipment affixes */
        float armor_penetration;       /* From weapon/enchants */
        float life_steal_percent;      /* From equipment procs/enchants */
        float mana_steal_percent;      /* From equipment procs/enchants */
        uint32_t elemental_damage;     /* Packed elemental damage bonuses */
        uint32_t status_immunities;    /* Packed status effect immunities */
        uint32_t active_set_bonuses;   /* Bitmask of active set bonuses */
    } RogueCombatEquipmentStats;

    /* Durability tracking for combat damage */
    typedef struct
    {
        uint8_t slot;                  /* Equipment slot (RogueEquipSlot) */
        uint16_t damage_taken;         /* Durability points lost */
        uint16_t remaining_durability; /* Current durability after damage */
        bool broken;                   /* Item broke during combat */
        uint32_t combat_event_id;      /* Associated combat event */
    } RogueEquipmentDurabilityEvent;

    /* Proc activation tracking */
    typedef struct
    {
        uint16_t proc_id;               /* Proc definition ID */
        uint8_t trigger_type;           /* RogueProcTrigger that fired */
        uint8_t stacks_applied;         /* Number of stacks added */
        uint16_t duration_remaining_ms; /* Remaining effect duration */
        int32_t magnitude;              /* Effect magnitude */
        uint32_t combat_context_id;     /* Combat event that triggered proc */
    } RogueEquipmentProcActivation;

    /* Set bonus state tracking */
    typedef struct
    {
        uint16_t set_id;         /* Equipment set ID */
        uint8_t pieces_equipped; /* Current pieces equipped */
        uint8_t bonus_tier;      /* Active bonus tier (0 = none) */
        uint32_t bonus_flags;    /* Bitmask of active bonus effects */
        bool just_activated;     /* Set bonus newly activated this frame */
        bool just_deactivated;   /* Set bonus lost this frame */
    } RogueEquipmentSetBonusState;

    /* Equipment weight impact on combat */
    typedef struct
    {
        float total_weight;             /* Total equipped weight */
        float weight_penalty;           /* Movement/timing penalty (0.0-1.0) */
        float attack_speed_modifier;    /* Attack speed adjustment */
        float dodge_speed_modifier;     /* Dodge/roll speed adjustment */
        float stamina_drain_multiplier; /* Stamina consumption multiplier */
        bool encumbered;                /* Over weight limit */
    } RogueEquipmentWeightImpact;

    /* Performance metrics */
    typedef struct
    {
        uint32_t stat_calculations_per_second;
        uint32_t durability_events_processed;
        uint32_t procs_triggered_total;
        uint32_t set_bonus_state_changes;
        uint32_t enchantment_applications;
        uint32_t weight_calculations_per_second;
        float average_stat_calc_time_us;
        float peak_stat_calc_time_us;
        uint32_t cache_hits;
        uint32_t cache_misses;
        time_t last_metrics_reset;
    } RogueCombatEquipBridgeMetrics;

    /* Main bridge structure */
    typedef struct RogueCombatEquipBridge
    {
        bool initialized;
        bool debug_logging;

        /* Real-time equipment stats cache */
        RogueCombatEquipmentStats cached_stats;
        bool stats_dirty;
        uint64_t last_equipment_change_timestamp;

        /* Durability tracking */
        RogueEquipmentDurabilityEvent durability_events[64];
        uint8_t durability_event_count;

        /* Active proc tracking */
        RogueEquipmentProcActivation active_procs[32];
        uint8_t active_proc_count;

        /* Set bonus state */
        RogueEquipmentSetBonusState set_bonuses[16]; /* Max 16 different sets */
        uint8_t set_bonus_count;

        /* Weight impact cache */
        RogueEquipmentWeightImpact weight_impact;
        bool weight_dirty;

        /* Performance settings */
        float stat_update_interval_ms;   /* How often to recalc stats */
        float weight_update_interval_ms; /* How often to recalc weight */
        uint32_t max_durability_events_per_frame;
        uint32_t max_proc_activations_per_frame;

        /* Performance metrics */
        RogueCombatEquipBridgeMetrics metrics;

    } RogueCombatEquipBridge;

    /* === Phase 3.2 API Functions === */

    /* 3.2.1 Real-time equipment stat application */
    int rogue_combat_equip_bridge_init(RogueCombatEquipBridge* bridge);
    void rogue_combat_equip_bridge_shutdown(RogueCombatEquipBridge* bridge);
    int rogue_combat_equip_bridge_update_stats(RogueCombatEquipBridge* bridge,
                                               struct RoguePlayer* player);
    int rogue_combat_equip_bridge_get_combat_stats(RogueCombatEquipBridge* bridge,
                                                   RogueCombatEquipmentStats* out_stats);
    int rogue_combat_equip_bridge_apply_stats_to_combat(RogueCombatEquipBridge* bridge,
                                                        struct RoguePlayerCombat* combat);

    /* 3.2.2 Equipment durability reduction hooks */
    int rogue_combat_equip_bridge_on_damage_taken(RogueCombatEquipBridge* bridge,
                                                  struct RoguePlayer* player,
                                                  uint32_t damage_amount, uint8_t damage_type);
    int rogue_combat_equip_bridge_on_attack_made(RogueCombatEquipBridge* bridge,
                                                 struct RoguePlayer* player, bool hit_target);
    int rogue_combat_equip_bridge_process_durability_events(RogueCombatEquipBridge* bridge);

    /* 3.2.3 Equipment proc effect triggers */
    int rogue_combat_equip_bridge_trigger_procs(RogueCombatEquipBridge* bridge,
                                                uint8_t trigger_type, uint32_t context_data);
    int rogue_combat_equip_bridge_update_active_procs(RogueCombatEquipBridge* bridge, float dt_ms);
    int rogue_combat_equip_bridge_get_active_procs(RogueCombatEquipBridge* bridge,
                                                   RogueEquipmentProcActivation* out_procs,
                                                   uint8_t max_procs);

    /* 3.2.4 Equipment set bonus activation/deactivation */
    int rogue_combat_equip_bridge_update_set_bonuses(RogueCombatEquipBridge* bridge,
                                                     struct RoguePlayer* player);
    int rogue_combat_equip_bridge_get_set_bonuses(RogueCombatEquipBridge* bridge,
                                                  RogueEquipmentSetBonusState* out_bonuses,
                                                  uint8_t max_bonuses);
    int rogue_combat_equip_bridge_apply_set_bonuses_to_combat(RogueCombatEquipBridge* bridge,
                                                              struct RoguePlayerCombat* combat);

    /* 3.2.5 Equipment enchantment effects integration */
    int rogue_combat_equip_bridge_apply_enchantments(RogueCombatEquipBridge* bridge,
                                                     struct RoguePlayer* player,
                                                     float* damage_multiplier,
                                                     uint32_t* elemental_damage);
    int rogue_combat_equip_bridge_trigger_enchantment_effects(RogueCombatEquipBridge* bridge,
                                                              uint8_t enchant_trigger,
                                                              uint32_t context_data);

    /* 3.2.6 Equipment weight impact on combat timing & movement */
    int rogue_combat_equip_bridge_update_weight_impact(RogueCombatEquipBridge* bridge,
                                                       struct RoguePlayer* player);
    int rogue_combat_equip_bridge_get_weight_impact(RogueCombatEquipBridge* bridge,
                                                    RogueEquipmentWeightImpact* out_impact);
    int rogue_combat_equip_bridge_apply_weight_to_combat(RogueCombatEquipBridge* bridge,
                                                         struct RoguePlayerCombat* combat);

    /* 3.2.7 Equipment upgrade notifications */
    int rogue_combat_equip_bridge_on_equipment_upgraded(RogueCombatEquipBridge* bridge,
                                                        uint8_t slot, uint32_t old_item_id,
                                                        uint32_t new_item_id);
    int rogue_combat_equip_bridge_on_equipment_enchanted(RogueCombatEquipBridge* bridge,
                                                         uint8_t slot, uint32_t enchant_id);
    int rogue_combat_equip_bridge_on_equipment_socketed(RogueCombatEquipBridge* bridge,
                                                        uint8_t slot, uint32_t gem_id);

    /* Performance & Debug */
    int rogue_combat_equip_bridge_get_metrics(RogueCombatEquipBridge* bridge,
                                              RogueCombatEquipBridgeMetrics* out_metrics);
    void rogue_combat_equip_bridge_reset_metrics(RogueCombatEquipBridge* bridge);
    int rogue_combat_equip_bridge_check_performance_thresholds(RogueCombatEquipBridge* bridge);
    void rogue_combat_equip_bridge_set_debug_logging(RogueCombatEquipBridge* bridge, bool enabled);
    int rogue_combat_equip_bridge_get_debug_status(RogueCombatEquipBridge* bridge);
    int rogue_combat_equip_bridge_validate(RogueCombatEquipBridge* bridge);

#ifdef __cplusplus
}
#endif
