/**
 * Phase 3.10 Persistence System ↔ All Systems Integration Bridge
 * (Header created separately due to apply patch path issue)
 */
#ifndef ROGUE_PERSISTENCE_INTEGRATION_BRIDGE_H
#define ROGUE_PERSISTENCE_INTEGRATION_BRIDGE_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RoguePersistenceBridgeMetrics
    {
        uint64_t events_processed;
        uint64_t components_marked;
        unsigned last_save_rc;
        uint32_t last_save_bytes;
        double last_save_ms;
        unsigned sections_reused;
        unsigned sections_written;
    } RoguePersistenceBridgeMetrics;

    typedef struct RoguePersistenceBridge
    {
        int initialized;
        RoguePersistenceBridgeMetrics metrics;
        uint32_t sub_item_pickup, sub_xp, sub_level, sub_skill_unlock;
        uint32_t sub_trade, sub_currency, sub_area, sub_config_reload;
    } RoguePersistenceBridge;

    int rogue_persist_bridge_init(RoguePersistenceBridge* b);
    void rogue_persist_bridge_shutdown(RoguePersistenceBridge* b);
    int rogue_persist_bridge_is_initialized(const RoguePersistenceBridge* b);
    int rogue_persist_bridge_save_slot(RoguePersistenceBridge* b, int slot_index);
    int rogue_persist_bridge_autosave(RoguePersistenceBridge* b, int slot_index);
    int rogue_persist_bridge_quicksave(RoguePersistenceBridge* b);
    int rogue_persist_bridge_enable_incremental(int enabled);
    int rogue_persist_bridge_enable_compression(int enabled, int min_bytes);
    int rogue_persist_bridge_validate_slot(int slot_index);
    uint32_t rogue_persist_bridge_last_tamper_flags(void);
    RoguePersistenceBridgeMetrics rogue_persist_bridge_get_metrics(const RoguePersistenceBridge* b);
    int rogue_persist_bridge_component_dirty(int component_id);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_PERSISTENCE_INTEGRATION_BRIDGE_H */
