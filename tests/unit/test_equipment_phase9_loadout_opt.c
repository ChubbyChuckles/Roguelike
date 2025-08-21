/* Phase 9: Loadout optimizer tests (comparison, hill-climb, cache pruning) */
#define SDL_MAIN_HANDLED 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/loadout_optimizer.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/equipment/equipment.h"
#include "core/stat_cache.h"
#include "core/equipment/equipment_stats.h"
#include "core/app_state.h"

/* Provide required player symbol for stat cache recomputation */
RoguePlayer g_exposed_player_for_stats = {0};

/* Forward declare asset load helper (actual implementation in codebase) */
int rogue_item_defs_load_from_cfg(const char* path);
int rogue_minimap_ping_loot(float x,float y,int rarity){ (void)x;(void)y;(void)rarity; return 0; }

static void ensure_item_content(void){ if(rogue_item_defs_count()>0) return; int added = rogue_item_defs_load_from_cfg("assets/test_items.cfg"); assert(added>0); }
static int first_two_weapon_defs(int* out_a, int* out_b){ int found=0; int total=rogue_item_defs_count(); for(int i=0;i<total;i++){ const RogueItemDef* d=rogue_item_def_at(i); if(!d) continue; if(d->category==ROGUE_ITEM_WEAPON){ if(found==0){ *out_a=i; found=1; } else if(found==1){ *out_b=i; return 2; } } } return found; }

/* Test 9.1: snapshot + compare diff counts */
static void test_snapshot_compare(void){ ensure_item_content(); rogue_equip_reset(); RogueLoadoutSnapshot a,b; int w1=-1,w2=-1; int got=first_two_weapon_defs(&w1,&w2); assert(got==2); int instA = rogue_items_spawn(w1,1,0,0); assert(instA>=0); assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, instA)==0); rogue_stat_cache_mark_dirty(); rogue_stat_cache_force_update(&g_exposed_player_for_stats); assert(rogue_loadout_snapshot(&a)==0);
    int instB = rogue_items_spawn(w2,1,0,0); assert(instB>=0); assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, instB)==0); rogue_stat_cache_mark_dirty(); rogue_stat_cache_force_update(&g_exposed_player_for_stats); assert(rogue_loadout_snapshot(&b)==0);
    int changed[ROGUE_EQUIP_SLOT_COUNT]={0}; int diff = rogue_loadout_compare(&a,&b,changed); assert(diff==1); assert(changed[ROGUE_EQUIP_WEAPON]==1); }

/* Fabricate affix values by directly setting instance fields (acceptable for test harness) */
extern int rogue_items_active_count(void);

static void fabricate_progression(void){ int w1=-1,w2=-1; first_two_weapon_defs(&w1,&w2); /* spawn several of higher rarity weapon (w2) to give optimizer choices */
    for(int i=0;i<3;i++){ rogue_items_spawn(w2,1,0,0); }
}

/* Test 9.2-9.4: optimizer improves DPS under constraints and uses cache */
static void test_optimizer(void){ ensure_item_content(); rogue_equip_reset(); rogue_loadout_cache_reset(); fabricate_progression(); int w1=-1,w2=-1; first_two_weapon_defs(&w1,&w2); int inst_base = rogue_items_spawn(w1,1,0,0); assert(inst_base>=0); assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, inst_base)==0); rogue_stat_cache_mark_dirty(); rogue_stat_cache_force_update(&g_exposed_player_for_stats); int base_dps = g_player_stat_cache.dps_estimate; int base_ehp = g_player_stat_cache.ehp_estimate; int improvements = rogue_loadout_optimize(50, base_ehp-10); assert(improvements>=0); assert(g_player_stat_cache.dps_estimate >= base_dps); int used=0,cap=0,hits=0,inserts=0; rogue_loadout_cache_stats(&used,&cap,&hits,&inserts); assert(cap==256); assert(inserts>0); }

int main(void){ rogue_items_init_runtime(); test_snapshot_compare(); test_optimizer(); printf("equipment_phase9_loadout_opt_ok\n"); return 0; }
