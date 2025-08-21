/* Phase 15: Integrity & Anti-Duplication tests (15.2 hash chain, 15.3 GUID uniqueness, 15.1 validation stub) */
#define SDL_MAIN_HANDLED 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/equipment/equipment.h"
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_instances.h"
#include "core/stat_cache.h"
#include "core/equipment/equipment_persist.h"
#include "core/app_state.h"

/* Provide stat recompute symbol */
RoguePlayer g_exposed_player_for_stats = {0};
int rogue_item_defs_load_from_cfg(const char* path);
int rogue_minimap_ping_loot(float x,float y,int rarity){(void)x;(void)y;(void)rarity;return 0;}

static void ensure_defs(void){ if(rogue_item_defs_count()>0) return; int added=rogue_item_defs_load_from_cfg("assets/test_items.cfg"); assert(added>0); }

/* Helper to find first N active instance GUIDs */
static int collect_guids(unsigned long long* out, int cap){ int c=0; for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP && c<cap;i++){ const RogueItemInstance* it=rogue_item_instance_at(i); if(it){ out[c++]=it->guid; } } return c; }

static void test_guid_uniqueness(void){ ensure_defs(); rogue_items_init_runtime(); int def0 = 0; int spawned=0; for(int i=0;i<8;i++){ int inst=rogue_items_spawn(def0,1,(float)i,0.f); assert(inst>=0); spawned++; }
    unsigned long long guids[16]; int gcount=collect_guids(guids,16); assert(gcount==spawned); for(int i=0;i<gcount;i++) for(int j=i+1;j<gcount;j++) assert(guids[i]!=guids[j]); }

static void test_equip_hash_chain_progress(void){ ensure_defs(); rogue_items_init_runtime(); rogue_equip_reset(); int inst=rogue_items_spawn(0,1,0,0); assert(inst>=0); unsigned long long g1 = rogue_item_instance_equip_chain(inst); assert(g1==0ULL); assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, inst)==0); unsigned long long g2 = rogue_item_instance_equip_chain(inst); assert(g2!=0ULL); int prev = rogue_equip_unequip(ROGUE_EQUIP_WEAPON); assert(prev==inst); unsigned long long g3 = rogue_item_instance_equip_chain(inst); assert(g3!=0ULL && g3!=g2); }

int main(void){ rogue_items_init_runtime(); test_guid_uniqueness(); test_equip_hash_chain_progress(); printf("equipment_phase15_integrity_ok\n"); return 0; }
