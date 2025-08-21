/**
 * Phase 3.9 UI System ↔ All Game Systems Integration Bridge
 *
 * Provides real-time binding between underlying gameplay systems (player stats,
 * inventory, combat log, skill progression, vendor/economy, crafting, world map)
 * and the UI layer without creating direct tight coupling. All code is written
 * in C for maximal portability (no C++ features used in this header / bridge).
 */

#ifndef ROGUE_UI_INTEGRATION_BRIDGE_H
#define ROGUE_UI_INTEGRATION_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* ------------------------- Public Constants ---------------------------- */

#define ROGUE_UI_MAX_BINDINGS 256
#define ROGUE_UI_MAX_COMBAT_LOG_ENTRIES 128
#define ROGUE_UI_MAX_INVENTORY_CHANGES 128
#define ROGUE_UI_MAX_SKILL_EVENTS 128
#define ROGUE_UI_MAX_VENDOR_EVENTS 64
#define ROGUE_UI_MAX_CRAFTING_EVENTS 64
#define ROGUE_UI_MAX_WORLDMAP_UPDATES 128

    /* System identifiers used for event source tracking */
    enum
    {
        ROGUE_UI_SOURCE_PLAYER = 1,
        ROGUE_UI_SOURCE_INVENTORY = 2,
        ROGUE_UI_SOURCE_COMBAT = 3,
        ROGUE_UI_SOURCE_SKILL = 4,
        ROGUE_UI_SOURCE_VENDOR = 5,
        ROGUE_UI_SOURCE_CRAFTING = 6,
        ROGUE_UI_SOURCE_WORLDMAP = 7
    };

    /* ------------------------- Data Structures ----------------------------- */

    typedef enum RogueUIBindingType
    {
        ROGUE_UI_BIND_HEALTH,
        ROGUE_UI_BIND_MANA,
        ROGUE_UI_BIND_XP,
        ROGUE_UI_BIND_LEVEL,
        ROGUE_UI_BIND_STAT_STRENGTH,
        ROGUE_UI_BIND_STAT_DEXTERITY,
        ROGUE_UI_BIND_STAT_INTELLIGENCE,
        ROGUE_UI_BIND_INVENTORY_COUNT,
        ROGUE_UI_BIND_GOLD,
        ROGUE_UI_BIND_CRAFTING_MATERIAL_COUNT,
        ROGUE_UI_BIND_SKILL_POINTS,
        ROGUE_UI_BIND_DISCOVERED_AREAS,
        ROGUE_UI_BIND_ACTIVE_QUESTS,
        ROGUE_UI_BIND_COUNT
    } RogueUIBindingType;

    typedef struct RogueUIBinding
    {
        RogueUIBindingType type;
        uint32_t last_value_u32;    /* Canonical numeric snapshot */
        float last_value_f;         /* Float snapshot when needed */
        bool dirty;                 /* Needs UI refresh */
        uint64_t last_update_ts_us; /* For rate limiting / telemetry */
    } RogueUIBinding;

    typedef struct RogueUICombatLogEntry
    {
        uint64_t timestamp_us;
        uint32_t event_type; /* maps to combat event category */
        float value;         /* damage/heal amount */
        uint32_t source_id;
        uint32_t target_id;
        bool critical;
    } RogueUICombatLogEntry;

    typedef struct RogueUIInventoryChange
    {
        uint64_t timestamp_us;
        uint32_t item_id;
        int32_t delta; /* + acquired, - removed */
        uint32_t slot_index;
        bool equipped_state_change; /* indicates equip/unequip */
    } RogueUIInventoryChange;

    typedef struct RogueUISkillEvent
    {
        uint64_t timestamp_us;
        uint32_t skill_id;
        uint32_t event_kind; /* unlocked, upgraded, mastery */
        uint32_t new_level;
    } RogueUISkillEvent;

    typedef struct RogueUIVendorEvent
    {
        uint64_t timestamp_us;
        uint32_t vendor_id;
        uint32_t event_kind; /* price change, restock */
        uint32_t affected_item_id;
        int32_t price_delta;
    } RogueUIVendorEvent;

    typedef struct RogueUICraftingEvent
    {
        uint64_t timestamp_us;
        uint32_t recipe_id;
        uint32_t event_kind; /* discovered, craft_started, craft_completed */
        uint32_t quantity;
        bool success;
    } RogueUICraftingEvent;

    typedef struct RogueUIWorldMapUpdate
    {
        uint64_t timestamp_us;
        uint32_t area_id;
        uint32_t update_kind; /* discovered, quest_marker_added */
        float world_x;
        float world_y;
    } RogueUIWorldMapUpdate;

    typedef struct RogueUIBridgeMetrics
    {
        uint64_t total_events_processed;
        uint64_t total_bind_updates;
        uint64_t dropped_events;
        uint64_t combat_log_entries;
        uint64_t inventory_events;
        uint64_t skill_events;
        uint64_t vendor_events;
        uint64_t crafting_events;
        uint64_t worldmap_updates;
        uint64_t last_process_time_us;
    } RogueUIBridgeMetrics;

    typedef struct RogueUIBridge
    {
        bool initialized;
        bool enabled;
        RogueUIBinding bindings[ROGUE_UI_BIND_COUNT];
        uint32_t binding_count; /* constant = ROGUE_UI_BIND_COUNT */

        /* Ring buffers / queues */
        RogueUICombatLogEntry combat_log[ROGUE_UI_MAX_COMBAT_LOG_ENTRIES];
        uint32_t combat_log_head;
        uint32_t combat_log_size;

        RogueUIInventoryChange inventory_changes[ROGUE_UI_MAX_INVENTORY_CHANGES];
        uint32_t inventory_head;
        uint32_t inventory_size;

        RogueUISkillEvent skill_events[ROGUE_UI_MAX_SKILL_EVENTS];
        uint32_t skill_head;
        uint32_t skill_size;

        RogueUIVendorEvent vendor_events[ROGUE_UI_MAX_VENDOR_EVENTS];
        uint32_t vendor_head;
        uint32_t vendor_size;

        RogueUICraftingEvent crafting_events[ROGUE_UI_MAX_CRAFTING_EVENTS];
        uint32_t crafting_head;
        uint32_t crafting_size;

        RogueUIWorldMapUpdate worldmap_updates[ROGUE_UI_MAX_WORLDMAP_UPDATES];
        uint32_t worldmap_head;
        uint32_t worldmap_size;

        /* Metrics */
        RogueUIBridgeMetrics metrics;

        /* Event subscription IDs (for cleanup) */
        uint32_t sub_player_move;
        uint32_t sub_player_attack;
        uint32_t sub_item_pickup;
        uint32_t sub_damage;
        uint32_t sub_level_up;
        uint32_t sub_xp_gain;
        uint32_t sub_currency_changed;
        uint32_t sub_trade_completed;
        uint32_t sub_area_entered;
        uint32_t sub_resource_spawned;
        uint32_t sub_skill_unlock;
        uint32_t sub_config_reload;
    } RogueUIBridge;

    /* ------------------------- API ---------------------------------------- */

    bool rogue_ui_bridge_init(RogueUIBridge* bridge);
    void rogue_ui_bridge_shutdown(RogueUIBridge* bridge);
    bool rogue_ui_bridge_update(RogueUIBridge* bridge, float dt);
    bool rogue_ui_bridge_is_operational(const RogueUIBridge* bridge);

    /* Binding interface */
    bool rogue_ui_bridge_get_binding(const RogueUIBridge* bridge, RogueUIBindingType type,
                                     RogueUIBinding* out_binding);
    bool rogue_ui_bridge_force_binding(RogueUIBridge* bridge, RogueUIBindingType type,
                                       uint32_t value_u32, float value_f);
    uint32_t rogue_ui_bridge_get_dirty_bindings(const RogueUIBridge* bridge,
                                                RogueUIBinding* out_array, uint32_t max_out);

    /* Combat log */
    uint32_t rogue_ui_bridge_get_combat_log(const RogueUIBridge* bridge,
                                            RogueUICombatLogEntry* out_entries,
                                            uint32_t max_entries);

    /* Inventory changes */
    uint32_t rogue_ui_bridge_get_inventory_changes(const RogueUIBridge* bridge,
                                                   RogueUIInventoryChange* out_changes,
                                                   uint32_t max_changes);

    /* Skill events */
    uint32_t rogue_ui_bridge_get_skill_events(const RogueUIBridge* bridge,
                                              RogueUISkillEvent* out_events, uint32_t max_events);

    /* Vendor events */
    uint32_t rogue_ui_bridge_get_vendor_events(const RogueUIBridge* bridge,
                                               RogueUIVendorEvent* out_events, uint32_t max_events);

    /* Crafting events */
    uint32_t rogue_ui_bridge_get_crafting_events(const RogueUIBridge* bridge,
                                                 RogueUICraftingEvent* out_events,
                                                 uint32_t max_events);

    /* World map updates */
    uint32_t rogue_ui_bridge_get_worldmap_updates(const RogueUIBridge* bridge,
                                                  RogueUIWorldMapUpdate* out_updates,
                                                  uint32_t max_updates);

    /* Metrics */
    RogueUIBridgeMetrics rogue_ui_bridge_get_metrics(const RogueUIBridge* bridge);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_UI_INTEGRATION_BRIDGE_H */
