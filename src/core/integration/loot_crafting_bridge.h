#ifndef ROGUE_LOOT_CRAFTING_BRIDGE_H
#define ROGUE_LOOT_CRAFTING_BRIDGE_H

#include "event_bus.h"
#include <stdbool.h>
#include <stdint.h>

/* Phase 3.4 Loot System ↔ Crafting System Bridge */

/* Loot-Crafting Bridge Event Types */
#define ROGUE_LOOT_CRAFT_EVENT_MATERIAL_SORTED 0x3401
#define ROGUE_LOOT_CRAFT_EVENT_RECIPE_DISCOVERED 0x3402
#define ROGUE_LOOT_CRAFT_EVENT_ITEM_SALVAGED 0x3403
#define ROGUE_LOOT_CRAFT_EVENT_RARE_MATERIAL_ALERT 0x3404
#define ROGUE_LOOT_CRAFT_EVENT_AUTO_QUEUED 0x3405
#define ROGUE_LOOT_CRAFT_EVENT_DEMAND_UPDATED 0x3406
#define ROGUE_LOOT_CRAFT_EVENT_QUALITY_BONUS_APPLIED 0x3407

/* Material Categories (Phase 3.4.1) */
typedef enum
{
    ROGUE_MATERIAL_BASIC_METALS,
    ROGUE_MATERIAL_RARE_METALS,
    ROGUE_MATERIAL_GEMS,
    ROGUE_MATERIAL_ORGANIC,
    ROGUE_MATERIAL_MAGICAL_ESSENCE,
    ROGUE_MATERIAL_CRAFTING_COMPONENTS,
    ROGUE_MATERIAL_COUNT
} RogueMaterialCategory;

/* Loot Material Sorting Record (Phase 3.4.1) */
typedef struct
{
    uint32_t item_id;
    RogueMaterialCategory category;
    uint16_t quantity;
    uint8_t quality_tier; /* 1=common, 2=uncommon, 3=rare, 4=epic, 5=legendary */
    uint32_t estimated_value;
    bool auto_sorted;
    uint32_t loot_event_id;
} RogueLootMaterialSort;

/* Loot Quality Influence (Phase 3.4.2) */
typedef struct
{
    uint8_t base_quality;         /* Quality of the looted material */
    float success_rate_modifier;  /* How much it affects crafting success */
    float output_quality_bonus;   /* Bonus to crafted item quality */
    uint32_t durability_bonus;    /* Extra durability for crafted items */
    bool enables_special_effects; /* Can unlock special crafting effects */
} RogueLootQualityInfluence;

/* Recipe Discovery Event (Phase 3.4.3) */
typedef struct
{
    uint32_t recipe_id;
    char recipe_name[64];
    uint32_t trigger_item_id; /* Item that triggered discovery */
    uint8_t discovery_method; /* 0=loot find, 1=material combo, 2=special event */
    uint16_t required_skill_level;
    uint32_t discovery_timestamp;
    bool is_rare_recipe;
} RogueLootRecipeDiscovery;

/* Salvage Material Generation (Phase 3.4.4) */
typedef struct
{
    uint32_t source_item_id;
    uint32_t salvage_material_ids[8]; /* Up to 8 different materials */
    uint16_t salvage_quantities[8];
    uint8_t salvage_count;
    float salvage_efficiency; /* 0.0 to 1.0, how much material recovered */
    uint32_t salvage_xp_gained;
    bool rare_component_found;
} RogueLootSalvageGeneration;

/* Rare Material Alert (Phase 3.4.5) */
typedef struct
{
    uint32_t material_id;
    char material_name[64];
    uint8_t rarity_level; /* 1-5, with 5 being legendary */
    uint32_t estimated_market_value;
    uint8_t crafting_potential; /* How many recipes this could improve */
    bool demand_spike_active;   /* Is this material in high demand? */
    uint32_t alert_priority;    /* 1=low, 2=medium, 3=high, 4=critical */
} RogueLootRareMaterialAlert;

/* Auto-Crafting Queue Entry (Phase 3.4.6) */
typedef struct
{
    uint32_t recipe_id;
    uint16_t quantity_to_craft;
    uint32_t material_requirements[16]; /* Material IDs needed */
    uint16_t material_quantities[16];   /* How much of each */
    uint8_t requirement_count;
    bool auto_start_when_ready;
    uint8_t priority_level; /* 1=low, 5=high */
    uint32_t queue_timestamp;
} RogueLootAutoCraftingQueue;

/* Material Demand Influence (Phase 3.4.7) */
typedef struct
{
    uint32_t material_id;
    float demand_multiplier;     /* 1.0 = normal, >1.0 = high demand */
    float drop_weight_modifier;  /* How much this affects loot tables */
    uint32_t recent_consumption; /* How much was used recently */
    uint32_t projected_need;     /* AI prediction of future need */
    uint16_t market_trend_days;  /* How many days this trend has lasted */
    bool shortage_warning;       /* Is supply running low? */
} RogueMaterialDemandInfluence;

/* Main Loot-Crafting Bridge Structure */
typedef struct
{
    bool initialized;
    uint64_t last_update_timestamp;

    /* Phase 3.4.1: Automatic Material Sorting */
    RogueLootMaterialSort material_sorts[64];
    uint8_t material_sort_count;
    uint32_t total_materials_sorted;
    bool auto_sorting_enabled[ROGUE_MATERIAL_COUNT];

    /* Phase 3.4.2: Quality Influence System */
    RogueLootQualityInfluence quality_influences[32];
    uint8_t quality_influence_count;
    float global_quality_bonus;

    /* Phase 3.4.3: Recipe Discovery System */
    RogueLootRecipeDiscovery discovered_recipes[128];
    uint8_t discovered_recipe_count;
    uint32_t total_recipes_discovered;

    /* Phase 3.4.4: Salvage Material Generation */
    RogueLootSalvageGeneration salvage_operations[32];
    uint8_t salvage_operation_count;
    uint32_t total_items_salvaged;
    float salvage_skill_bonus;

    /* Phase 3.4.5: Rare Material Alerts */
    RogueLootRareMaterialAlert rare_material_alerts[16];
    uint8_t rare_alert_count;
    bool alert_notifications_enabled;
    uint8_t alert_threshold_rarity; /* Minimum rarity level to trigger alerts */

    /* Phase 3.4.6: Auto-Crafting Queue */
    RogueLootAutoCraftingQueue auto_craft_queue[32];
    uint8_t auto_craft_queue_count;
    bool auto_crafting_enabled;
    uint32_t total_auto_crafts_completed;

    /* Phase 3.4.7: Demand Influence System */
    RogueMaterialDemandInfluence demand_influences[ROGUE_MATERIAL_COUNT * 8];
    uint8_t demand_influence_count;
    bool dynamic_drops_enabled;

    /* Performance & Debug Metrics */
    struct
    {
        uint32_t materials_sorted_total;
        uint32_t quality_bonuses_applied;
        uint32_t recipes_discovered_total;
        uint32_t salvage_operations_completed;
        uint32_t rare_alerts_triggered;
        uint32_t auto_crafts_queued;
        uint32_t demand_updates_processed;
        float avg_processing_time_ms;
        uint64_t total_processing_time_us;
    } metrics;

    /* Debug & Logging */
    bool debug_mode;
    char last_error[128];
    uint32_t error_count;

} RogueLootCraftingBridge;

/* Phase 3.4.1: Automatic Material Sorting Functions */
int rogue_loot_crafting_bridge_init(RogueLootCraftingBridge* bridge);
void rogue_loot_crafting_bridge_shutdown(RogueLootCraftingBridge* bridge);
int rogue_loot_crafting_bridge_update(RogueLootCraftingBridge* bridge, float dt_ms);

int rogue_loot_crafting_bridge_sort_material(RogueLootCraftingBridge* bridge, uint32_t item_id,
                                             RogueMaterialCategory category, uint16_t quantity,
                                             uint8_t quality_tier, uint32_t loot_event_id);

int rogue_loot_crafting_bridge_enable_auto_sorting(RogueLootCraftingBridge* bridge,
                                                   RogueMaterialCategory category, bool enabled);

/* Phase 3.4.2: Quality Influence Functions */
int rogue_loot_crafting_bridge_apply_quality_influence(RogueLootCraftingBridge* bridge,
                                                       uint8_t material_quality,
                                                       float* success_rate_modifier,
                                                       float* quality_bonus,
                                                       uint32_t* durability_bonus);

int rogue_loot_crafting_bridge_calculate_quality_bonus(RogueLootCraftingBridge* bridge,
                                                       uint32_t* material_ids,
                                                       uint8_t* material_qualities,
                                                       uint8_t material_count,
                                                       float* out_total_bonus);

/* Phase 3.4.3: Recipe Discovery Functions */
int rogue_loot_crafting_bridge_discover_recipe_from_loot(RogueLootCraftingBridge* bridge,
                                                         uint32_t trigger_item_id,
                                                         uint32_t recipe_id,
                                                         const char* recipe_name, bool is_rare);

int rogue_loot_crafting_bridge_get_discovered_recipes(RogueLootCraftingBridge* bridge,
                                                      RogueLootRecipeDiscovery* out_recipes,
                                                      uint8_t max_count, bool only_recent);

/* Phase 3.4.4: Salvage Material Generation Functions */
int rogue_loot_crafting_bridge_salvage_item(RogueLootCraftingBridge* bridge,
                                            uint32_t source_item_id, uint32_t* material_ids,
                                            uint16_t* material_quantities, uint8_t material_count,
                                            float efficiency);

int rogue_loot_crafting_bridge_get_salvage_preview(RogueLootCraftingBridge* bridge,
                                                   uint32_t item_id, uint32_t* out_material_ids,
                                                   uint16_t* out_quantities, uint8_t max_materials);

/* Phase 3.4.5: Rare Material Alert Functions */
int rogue_loot_crafting_bridge_trigger_rare_material_alert(RogueLootCraftingBridge* bridge,
                                                           uint32_t material_id,
                                                           const char* material_name,
                                                           uint8_t rarity_level,
                                                           uint32_t market_value);

int rogue_loot_crafting_bridge_get_active_alerts(RogueLootCraftingBridge* bridge,
                                                 RogueLootRareMaterialAlert* out_alerts,
                                                 uint8_t max_count);

void rogue_loot_crafting_bridge_set_alert_threshold(RogueLootCraftingBridge* bridge,
                                                    uint8_t min_rarity_level);

/* Phase 3.4.6: Auto-Crafting Queue Functions */
int rogue_loot_crafting_bridge_queue_auto_craft(RogueLootCraftingBridge* bridge, uint32_t recipe_id,
                                                uint16_t quantity, uint32_t* required_materials,
                                                uint16_t* required_quantities,
                                                uint8_t material_count, uint8_t priority);

int rogue_loot_crafting_bridge_process_auto_craft_queue(RogueLootCraftingBridge* bridge);

int rogue_loot_crafting_bridge_get_queue_status(RogueLootCraftingBridge* bridge,
                                                RogueLootAutoCraftingQueue* out_queue,
                                                uint8_t max_entries);

/* Phase 3.4.7: Demand Influence Functions */
int rogue_loot_crafting_bridge_update_material_demand(RogueLootCraftingBridge* bridge,
                                                      uint32_t material_id,
                                                      uint32_t recent_consumption,
                                                      uint32_t projected_need);

int rogue_loot_crafting_bridge_get_demand_modifier(RogueLootCraftingBridge* bridge,
                                                   uint32_t material_id,
                                                   float* out_drop_weight_modifier);

int rogue_loot_crafting_bridge_apply_demand_to_loot_tables(RogueLootCraftingBridge* bridge,
                                                           uint32_t* material_ids,
                                                           float* drop_weights,
                                                           uint8_t material_count);

/* Debug & Utility Functions */
void rogue_loot_crafting_bridge_set_debug_mode(RogueLootCraftingBridge* bridge, bool enabled);
void rogue_loot_crafting_bridge_get_metrics(RogueLootCraftingBridge* bridge, char* out_buffer,
                                            size_t buffer_size);
void rogue_loot_crafting_bridge_reset_metrics(RogueLootCraftingBridge* bridge);

#endif /* ROGUE_LOOT_CRAFTING_BRIDGE_H */
