#include "persistence_integration_bridge.h"
#include "core/integration/event_bus.h"
#include "core/save_manager.h"
#include <string.h>
#include <stdio.h>

static void pib_mark(RoguePersistenceBridge* b, int comp){ if(rogue_save_mark_component_dirty(comp)==0){ b->metrics.components_marked++; } }
static bool on_item_pickup(const RogueEvent* ev, void* user){ (void)ev; RoguePersistenceBridge* b=user; b->metrics.events_processed++; pib_mark(b,ROGUE_SAVE_COMP_INVENTORY); pib_mark(b,ROGUE_SAVE_COMP_INV_ENTRIES); return true; }
static bool on_xp(const RogueEvent* ev, void* user){ (void)ev; RoguePersistenceBridge* b=user; b->metrics.events_processed++; pib_mark(b,ROGUE_SAVE_COMP_PLAYER); pib_mark(b,ROGUE_SAVE_COMP_SKILLS); return true; }
static bool on_level(const RogueEvent* ev, void* user){ (void)ev; RoguePersistenceBridge* b=user; b->metrics.events_processed++; pib_mark(b,ROGUE_SAVE_COMP_PLAYER); return true; }
static bool on_skill_unlock(const RogueEvent* ev, void* user){ (void)ev; RoguePersistenceBridge* b=user; b->metrics.events_processed++; pib_mark(b,ROGUE_SAVE_COMP_SKILLS); return true; }
static bool on_trade(const RogueEvent* ev, void* user){ (void)ev; RoguePersistenceBridge* b=user; b->metrics.events_processed++; pib_mark(b,ROGUE_SAVE_COMP_PLAYER); pib_mark(b,ROGUE_SAVE_COMP_INVENTORY); return true; }
static bool on_currency(const RogueEvent* ev, void* user){ (void)ev; RoguePersistenceBridge* b=user; b->metrics.events_processed++; pib_mark(b,ROGUE_SAVE_COMP_PLAYER); return true; }
static bool on_area(const RogueEvent* ev, void* user){ (void)ev; RoguePersistenceBridge* b=user; b->metrics.events_processed++; pib_mark(b,ROGUE_SAVE_COMP_WORLD_META); return true; }
static bool on_config_reload(const RogueEvent* ev, void* user){ (void)ev; RoguePersistenceBridge* b=user; b->metrics.events_processed++; pib_mark(b,ROGUE_SAVE_COMP_STRINGS); pib_mark(b,ROGUE_SAVE_COMP_WORLD_META); return true; }

int rogue_persist_bridge_init(RoguePersistenceBridge* b){
    if(!b) return -1; memset(b,0,sizeof *b);
    if(!rogue_event_bus_get_instance()){ RogueEventBusConfig cfg = rogue_event_bus_create_default_config("persist_bridge_bus"); rogue_event_bus_init(&cfg); }
    b->sub_item_pickup = rogue_event_subscribe(ROGUE_EVENT_ITEM_PICKED_UP,on_item_pickup,b,0);
    b->sub_xp = rogue_event_subscribe(ROGUE_EVENT_XP_GAINED,on_xp,b,0);
    b->sub_level = rogue_event_subscribe(ROGUE_EVENT_LEVEL_UP,on_level,b,0);
    b->sub_skill_unlock = rogue_event_subscribe(ROGUE_EVENT_SKILL_UNLOCKED,on_skill_unlock,b,0);
    b->sub_trade = rogue_event_subscribe(ROGUE_EVENT_TRADE_COMPLETED,on_trade,b,0);
    b->sub_currency = rogue_event_subscribe(ROGUE_EVENT_CURRENCY_CHANGED,on_currency,b,0);
    b->sub_area = rogue_event_subscribe(ROGUE_EVENT_AREA_ENTERED,on_area,b,0);
    b->sub_config_reload = rogue_event_subscribe(ROGUE_EVENT_CONFIG_RELOADED,on_config_reload,b,0);
    b->initialized=1; return 0;
}
void rogue_persist_bridge_shutdown(RoguePersistenceBridge* b){ if(!b) return; b->initialized=0; }
int rogue_persist_bridge_is_initialized(const RoguePersistenceBridge* b){ return b && b->initialized; }

static void harvest_reuse_metrics(RoguePersistenceBridge* b){ unsigned r=0,w=0; rogue_save_last_section_reuse(&r,&w); b->metrics.sections_reused=r; b->metrics.sections_written=w; }
static void publish_save_completed(int rc, double ms, uint32_t bytes){ (void)bytes; RogueEventPayload p; memset(&p,0,sizeof p); p.save_completed.success = (rc==0); p.save_completed.save_time_seconds = ms/1000.0; snprintf(p.save_completed.save_file,sizeof p.save_completed.save_file,"slot"); rogue_event_publish(ROGUE_EVENT_SAVE_COMPLETED,&p,ROGUE_EVENT_PRIORITY_LOW,0,"persist_bridge"); }

int rogue_persist_bridge_save_slot(RoguePersistenceBridge* b, int slot_index){ if(!b||!b->initialized) return -1; int rc=rogue_save_manager_save_slot(slot_index); b->metrics.last_save_rc=rc; b->metrics.last_save_bytes=rogue_save_last_save_bytes(); b->metrics.last_save_ms=rogue_save_last_save_ms(); harvest_reuse_metrics(b); publish_save_completed(rc,b->metrics.last_save_ms,b->metrics.last_save_bytes); return rc; }
int rogue_persist_bridge_autosave(RoguePersistenceBridge* b, int slot_index){ if(!b||!b->initialized) return -1; int rc=rogue_save_manager_autosave(slot_index); b->metrics.last_save_rc=rc; b->metrics.last_save_bytes=rogue_save_last_save_bytes(); b->metrics.last_save_ms=rogue_save_last_save_ms(); harvest_reuse_metrics(b); publish_save_completed(rc,b->metrics.last_save_ms,b->metrics.last_save_bytes); return rc; }
int rogue_persist_bridge_quicksave(RoguePersistenceBridge* b){ if(!b||!b->initialized) return -1; int rc=rogue_save_manager_quicksave(); b->metrics.last_save_rc=rc; b->metrics.last_save_bytes=rogue_save_last_save_bytes(); b->metrics.last_save_ms=rogue_save_last_save_ms(); harvest_reuse_metrics(b); publish_save_completed(rc,b->metrics.last_save_ms,b->metrics.last_save_bytes); return rc; }

int rogue_persist_bridge_enable_incremental(int enabled){ return rogue_save_set_incremental(enabled); }
int rogue_persist_bridge_enable_compression(int enabled, int min_bytes){ return rogue_save_set_compression(enabled,min_bytes); }

static int count_iter(const struct RogueSaveDescriptor* d, uint32_t id, const void* data, uint32_t size, void* user){ (void)d;(void)id;(void)data;(void)size; int* cnt=user; (*cnt)++; return 0; }
int rogue_persist_bridge_validate_slot(int slot_index){ int cnt=0; int rc=rogue_save_for_each_section(slot_index,count_iter,&cnt); if(rc!=0) return rc; return cnt; }
uint32_t rogue_persist_bridge_last_tamper_flags(void){ return rogue_save_last_tamper_flags(); }
RoguePersistenceBridgeMetrics rogue_persist_bridge_get_metrics(const RoguePersistenceBridge* b){ RoguePersistenceBridgeMetrics m; memset(&m,0,sizeof m); if(b) m=b->metrics; return m; }
int rogue_persist_bridge_component_dirty(int component_id){ return rogue_save_component_is_dirty(component_id); }
