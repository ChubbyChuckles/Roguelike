#include "ui_integration_bridge.h"
#include "event_bus.h"
#include <stdio.h>
#include <string.h>

/* Utility: timestamp */
static uint64_t ui_timestamp_us(void) { return rogue_event_get_timestamp_us(); }

static void ui_mark_binding_dirty(RogueUIBinding* b)
{
    if (!b)
        return;
    b->dirty = true;
    b->last_update_ts_us = ui_timestamp_us();
}

static void ui_reset_ring_uint32(uint32_t* head, uint32_t* size)
{
    *head = 0;
    *size = 0;
}

static void ui_push_combat(RogueUIBridge* bridge, const RogueUICombatLogEntry* e)
{
    if (!bridge || !e)
        return;
    if (bridge->combat_log_size < ROGUE_UI_MAX_COMBAT_LOG_ENTRIES)
    {
        uint32_t idx =
            (bridge->combat_log_head + bridge->combat_log_size) % ROGUE_UI_MAX_COMBAT_LOG_ENTRIES;
        bridge->combat_log[idx] = *e;
        bridge->combat_log_size++;
    }
    else
    { /* overwrite oldest */
        bridge->combat_log[bridge->combat_log_head] = *e;
        bridge->combat_log_head = (bridge->combat_log_head + 1) % ROGUE_UI_MAX_COMBAT_LOG_ENTRIES;
        bridge->metrics.dropped_events++;
    }
    bridge->metrics.combat_log_entries = bridge->combat_log_size;
}

static void ui_push_inventory(RogueUIBridge* bridge, const RogueUIInventoryChange* c)
{
    if (!bridge || !c)
        return;
    if (bridge->inventory_size < ROGUE_UI_MAX_INVENTORY_CHANGES)
    {
        uint32_t idx =
            (bridge->inventory_head + bridge->inventory_size) % ROGUE_UI_MAX_INVENTORY_CHANGES;
        bridge->inventory_changes[idx] = *c;
        bridge->inventory_size++;
    }
    else
    {
        bridge->inventory_changes[bridge->inventory_head] = *c;
        bridge->inventory_head = (bridge->inventory_head + 1) % ROGUE_UI_MAX_INVENTORY_CHANGES;
        bridge->metrics.dropped_events++;
    }
    bridge->metrics.inventory_events = bridge->inventory_size;
}

static void ui_push_skill(RogueUIBridge* bridge, const RogueUISkillEvent* s)
{
    if (!bridge || !s)
        return;
    if (bridge->skill_size < ROGUE_UI_MAX_SKILL_EVENTS)
    {
        uint32_t idx = (bridge->skill_head + bridge->skill_size) % ROGUE_UI_MAX_SKILL_EVENTS;
        bridge->skill_events[idx] = *s;
        bridge->skill_size++;
    }
    else
    {
        bridge->skill_events[bridge->skill_head] = *s;
        bridge->skill_head = (bridge->skill_head + 1) % ROGUE_UI_MAX_SKILL_EVENTS;
        bridge->metrics.dropped_events++;
    }
    bridge->metrics.skill_events = bridge->skill_size;
}

static void ui_push_vendor(RogueUIBridge* bridge, const RogueUIVendorEvent* v)
{
    if (!bridge || !v)
        return;
    if (bridge->vendor_size < ROGUE_UI_MAX_VENDOR_EVENTS)
    {
        uint32_t idx = (bridge->vendor_head + bridge->vendor_size) % ROGUE_UI_MAX_VENDOR_EVENTS;
        bridge->vendor_events[idx] = *v;
        bridge->vendor_size++;
    }
    else
    {
        bridge->vendor_events[bridge->vendor_head] = *v;
        bridge->vendor_head = (bridge->vendor_head + 1) % ROGUE_UI_MAX_VENDOR_EVENTS;
        bridge->metrics.dropped_events++;
    }
    bridge->metrics.vendor_events = bridge->vendor_size;
}

static void ui_push_crafting(RogueUIBridge* bridge, const RogueUICraftingEvent* c)
{
    if (!bridge || !c)
        return;
    if (bridge->crafting_size < ROGUE_UI_MAX_CRAFTING_EVENTS)
    {
        uint32_t idx =
            (bridge->crafting_head + bridge->crafting_size) % ROGUE_UI_MAX_CRAFTING_EVENTS;
        bridge->crafting_events[idx] = *c;
        bridge->crafting_size++;
    }
    else
    {
        bridge->crafting_events[bridge->crafting_head] = *c;
        bridge->crafting_head = (bridge->crafting_head + 1) % ROGUE_UI_MAX_CRAFTING_EVENTS;
        bridge->metrics.dropped_events++;
    }
    bridge->metrics.crafting_events = bridge->crafting_size;
}

static void ui_push_worldmap(RogueUIBridge* bridge, const RogueUIWorldMapUpdate* w)
{
    if (!bridge || !w)
        return;
    if (bridge->worldmap_size < ROGUE_UI_MAX_WORLDMAP_UPDATES)
    {
        uint32_t idx =
            (bridge->worldmap_head + bridge->worldmap_size) % ROGUE_UI_MAX_WORLDMAP_UPDATES;
        bridge->worldmap_updates[idx] = *w;
        bridge->worldmap_size++;
    }
    else
    {
        bridge->worldmap_updates[bridge->worldmap_head] = *w;
        bridge->worldmap_head = (bridge->worldmap_head + 1) % ROGUE_UI_MAX_WORLDMAP_UPDATES;
        bridge->metrics.dropped_events++;
    }
    bridge->metrics.worldmap_updates = bridge->worldmap_size;
}

/* ---------------- Event Callbacks (subscribe to bus) ------------------ */

static bool ui_on_player_moved(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    /* Mark discovered areas binding as potentially changed */
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_DISCOVERED_AREAS]);
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_item_picked(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    RogueUIInventoryChange change;
    memset(&change, 0, sizeof(change));
    change.timestamp_us = ui_timestamp_us();
    change.item_id = evt->payload.item_picked_up.item_id;
    change.delta = 1;
    change.slot_index = 0;
    change.equipped_state_change = false;
    ui_push_inventory(bridge, &change);
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_INVENTORY_COUNT]);
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_damage_event(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    RogueUICombatLogEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.timestamp_us = ui_timestamp_us();
    entry.event_type = evt->type_id;
    entry.value = evt->payload.damage_event.damage_amount;
    entry.source_id = evt->payload.damage_event.source_entity_id;
    entry.target_id = evt->payload.damage_event.target_entity_id;
    entry.critical = evt->payload.damage_event.is_critical;
    ui_push_combat(bridge, &entry);
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_HEALTH]);
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_level_up(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_LEVEL]);
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_XP]);
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_xp_gained(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    /* XP binding update only (value accumulation done elsewhere) */
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_XP]);
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_skill_unlocked(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    RogueUISkillEvent se;
    memset(&se, 0, sizeof(se));
    se.timestamp_us = ui_timestamp_us();
    se.skill_id = evt->payload.xp_gained.source_id; /* reuse field placeholder */
    se.event_kind = 1;
    se.new_level = 1;
    ui_push_skill(bridge, &se);
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_SKILL_POINTS]);
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_config_reloaded(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    /* Mark many bindings dirty to force refresh after config reload */
    for (int i = 0; i < ROGUE_UI_BIND_COUNT; i++)
        bridge->bindings[i].dirty = true;
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_currency_changed(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_GOLD]);
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_trade_completed(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    /* Trade may affect gold + inventory */
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_GOLD]);
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_INVENTORY_COUNT]);
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_area_entered(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_DISCOVERED_AREAS]);
    bridge->metrics.total_events_processed++;
    return true;
}

static bool ui_on_resource_spawned(const RogueEvent* evt, void* user)
{
    RogueUIBridge* bridge = (RogueUIBridge*) user;
    if (!bridge || !evt)
        return false;
    /* Potentially affects world map resource indicators */
    RogueUIWorldMapUpdate upd;
    memset(&upd, 0, sizeof(upd));
    upd.timestamp_us = ui_timestamp_us();
    upd.area_id = evt->payload.area_transition.area_id; /* reuse structure */
    upd.update_kind = 2;                                /* resource */
    upd.world_x = 0.0f;
    upd.world_y = 0.0f;
    ui_push_worldmap(bridge, &upd);
    ui_mark_binding_dirty(&bridge->bindings[ROGUE_UI_BIND_DISCOVERED_AREAS]);
    bridge->metrics.total_events_processed++;
    return true;
}

/* --------------------------- Public API -------------------------------- */

bool rogue_ui_bridge_init(RogueUIBridge* bridge)
{
    if (!bridge)
        return false;
    memset(bridge, 0, sizeof(*bridge));
    bridge->binding_count = ROGUE_UI_BIND_COUNT;
    bridge->enabled = true;
    bridge->initialized = true;
    /* Subscribe to event bus */
    RogueEventBus* bus = rogue_event_bus_get_instance();
    (void) bus; /* assume initialized elsewhere */
    bridge->sub_player_move = rogue_event_subscribe(ROGUE_EVENT_PLAYER_MOVED, ui_on_player_moved,
                                                    bridge, ROGUE_UI_SOURCE_PLAYER);
    bridge->sub_item_pickup = rogue_event_subscribe(ROGUE_EVENT_ITEM_PICKED_UP, ui_on_item_picked,
                                                    bridge, ROGUE_UI_SOURCE_INVENTORY);
    bridge->sub_damage = rogue_event_subscribe(ROGUE_EVENT_DAMAGE_DEALT, ui_on_damage_event, bridge,
                                               ROGUE_UI_SOURCE_COMBAT);
    bridge->sub_level_up =
        rogue_event_subscribe(ROGUE_EVENT_LEVEL_UP, ui_on_level_up, bridge, ROGUE_UI_SOURCE_PLAYER);
    bridge->sub_xp_gain = rogue_event_subscribe(ROGUE_EVENT_XP_GAINED, ui_on_xp_gained, bridge,
                                                ROGUE_UI_SOURCE_PLAYER);
    bridge->sub_skill_unlock = rogue_event_subscribe(
        ROGUE_EVENT_SKILL_UNLOCKED, ui_on_skill_unlocked, bridge, ROGUE_UI_SOURCE_SKILL);
    bridge->sub_config_reload = rogue_event_subscribe(
        ROGUE_EVENT_CONFIG_RELOADED, ui_on_config_reloaded, bridge, ROGUE_UI_SOURCE_PLAYER);
    bridge->sub_currency_changed = rogue_event_subscribe(
        ROGUE_EVENT_CURRENCY_CHANGED, ui_on_currency_changed, bridge, ROGUE_UI_SOURCE_VENDOR);
    bridge->sub_trade_completed = rogue_event_subscribe(
        ROGUE_EVENT_TRADE_COMPLETED, ui_on_trade_completed, bridge, ROGUE_UI_SOURCE_VENDOR);
    bridge->sub_area_entered = rogue_event_subscribe(ROGUE_EVENT_AREA_ENTERED, ui_on_area_entered,
                                                     bridge, ROGUE_UI_SOURCE_WORLDMAP);
    bridge->sub_resource_spawned = rogue_event_subscribe(
        ROGUE_EVENT_RESOURCE_SPAWNED, ui_on_resource_spawned, bridge, ROGUE_UI_SOURCE_WORLDMAP);
    return true;
}

void rogue_ui_bridge_shutdown(RogueUIBridge* bridge)
{
    if (!bridge || !bridge->initialized)
        return;
    rogue_event_unsubscribe(bridge->sub_player_move);
    rogue_event_unsubscribe(bridge->sub_item_pickup);
    rogue_event_unsubscribe(bridge->sub_damage);
    rogue_event_unsubscribe(bridge->sub_level_up);
    rogue_event_unsubscribe(bridge->sub_xp_gain);
    rogue_event_unsubscribe(bridge->sub_skill_unlock);
    rogue_event_unsubscribe(bridge->sub_config_reload);
    rogue_event_unsubscribe(bridge->sub_currency_changed);
    rogue_event_unsubscribe(bridge->sub_trade_completed);
    rogue_event_unsubscribe(bridge->sub_area_entered);
    rogue_event_unsubscribe(bridge->sub_resource_spawned);
    bridge->initialized = false;
    bridge->enabled = false;
}

bool rogue_ui_bridge_update(RogueUIBridge* bridge, float dt)
{
    (void) dt;
    if (!bridge || !bridge->initialized || !bridge->enabled)
        return false;
    /* Mark metrics last process time */
    bridge->metrics.last_process_time_us = ui_timestamp_us();
    return true;
}

bool rogue_ui_bridge_is_operational(const RogueUIBridge* bridge)
{
    return bridge && bridge->initialized && bridge->enabled;
}

bool rogue_ui_bridge_get_binding(const RogueUIBridge* bridge, RogueUIBindingType type,
                                 RogueUIBinding* out_binding)
{
    if (!bridge || !out_binding || type >= ROGUE_UI_BIND_COUNT)
        return false;
    *out_binding = bridge->bindings[type];
    return true;
}

bool rogue_ui_bridge_force_binding(RogueUIBridge* bridge, RogueUIBindingType type,
                                   uint32_t value_u32, float value_f)
{
    if (!bridge || type >= ROGUE_UI_BIND_COUNT)
        return false;
    bridge->bindings[type].last_value_u32 = value_u32;
    bridge->bindings[type].last_value_f = value_f;
    ui_mark_binding_dirty(&bridge->bindings[type]);
    bridge->metrics.total_bind_updates++;
    return true;
}

uint32_t rogue_ui_bridge_get_dirty_bindings(const RogueUIBridge* bridge, RogueUIBinding* out_array,
                                            uint32_t max_out)
{
    if (!bridge || !out_array || max_out == 0)
        return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < ROGUE_UI_BIND_COUNT && count < max_out; i++)
    {
        if (bridge->bindings[i].dirty)
        {
            out_array[count++] = bridge->bindings[i];
        }
    }
    return count;
}

uint32_t rogue_ui_bridge_get_combat_log(const RogueUIBridge* bridge,
                                        RogueUICombatLogEntry* out_entries, uint32_t max_entries)
{
    if (!bridge || !out_entries || max_entries == 0)
        return 0;
    uint32_t copied = 0;
    for (uint32_t i = 0; i < bridge->combat_log_size && copied < max_entries; i++)
    {
        uint32_t idx = (bridge->combat_log_head + i) % ROGUE_UI_MAX_COMBAT_LOG_ENTRIES;
        out_entries[copied++] = bridge->combat_log[idx];
    }
    return copied;
}

uint32_t rogue_ui_bridge_get_inventory_changes(const RogueUIBridge* bridge,
                                               RogueUIInventoryChange* out_changes,
                                               uint32_t max_changes)
{
    if (!bridge || !out_changes || max_changes == 0)
        return 0;
    uint32_t copied = 0;
    for (uint32_t i = 0; i < bridge->inventory_size && copied < max_changes; i++)
    {
        uint32_t idx = (bridge->inventory_head + i) % ROGUE_UI_MAX_INVENTORY_CHANGES;
        out_changes[copied++] = bridge->inventory_changes[idx];
    }
    return copied;
}

uint32_t rogue_ui_bridge_get_skill_events(const RogueUIBridge* bridge,
                                          RogueUISkillEvent* out_events, uint32_t max_events)
{
    if (!bridge || !out_events || max_events == 0)
        return 0;
    uint32_t copied = 0;
    for (uint32_t i = 0; i < bridge->skill_size && copied < max_events; i++)
    {
        uint32_t idx = (bridge->skill_head + i) % ROGUE_UI_MAX_SKILL_EVENTS;
        out_events[copied++] = bridge->skill_events[idx];
    }
    return copied;
}

uint32_t rogue_ui_bridge_get_vendor_events(const RogueUIBridge* bridge,
                                           RogueUIVendorEvent* out_events, uint32_t max_events)
{
    if (!bridge || !out_events || max_events == 0)
        return 0;
    uint32_t copied = 0;
    for (uint32_t i = 0; i < bridge->vendor_size && copied < max_events; i++)
    {
        uint32_t idx = (bridge->vendor_head + i) % ROGUE_UI_MAX_VENDOR_EVENTS;
        out_events[copied++] = bridge->vendor_events[idx];
    }
    return copied;
}

uint32_t rogue_ui_bridge_get_crafting_events(const RogueUIBridge* bridge,
                                             RogueUICraftingEvent* out_events, uint32_t max_events)
{
    if (!bridge || !out_events || max_events == 0)
        return 0;
    uint32_t copied = 0;
    for (uint32_t i = 0; i < bridge->crafting_size && copied < max_events; i++)
    {
        uint32_t idx = (bridge->crafting_head + i) % ROGUE_UI_MAX_CRAFTING_EVENTS;
        out_events[copied++] = bridge->crafting_events[idx];
    }
    return copied;
}

uint32_t rogue_ui_bridge_get_worldmap_updates(const RogueUIBridge* bridge,
                                              RogueUIWorldMapUpdate* out_updates,
                                              uint32_t max_updates)
{
    if (!bridge || !out_updates || max_updates == 0)
        return 0;
    uint32_t copied = 0;
    for (uint32_t i = 0; i < bridge->worldmap_size && copied < max_updates; i++)
    {
        uint32_t idx = (bridge->worldmap_head + i) % ROGUE_UI_MAX_WORLDMAP_UPDATES;
        out_updates[copied++] = bridge->worldmap_updates[idx];
    }
    return copied;
}

RogueUIBridgeMetrics rogue_ui_bridge_get_metrics(const RogueUIBridge* bridge)
{
    RogueUIBridgeMetrics m;
    memset(&m, 0, sizeof(m));
    if (!bridge)
        return m;
    return bridge->metrics;
}
