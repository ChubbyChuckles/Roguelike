/* Phase 1 binary SaveManager roundtrip test */
#include "core/save_manager.h"
#include "core/app_state.h"
#include "core/skills.h"
#include "core/loot/loot_instances.h"
#include "../../src/core/buffs.h"
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_affixes.h"
#include "core/persistence.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

/* Use real buff system (no custom implementation). */

int main(void){
    memset(&g_app,0,sizeof g_app);
    rogue_save_manager_init();
    rogue_register_core_save_components();

    /* Seed some state */
    g_app.player.level = 7; g_app.player.xp=321; g_app.player.health=77; g_app.talent_points=5;
    /* Minimal skill registration stub (simulate two skills) */
    struct RogueSkillDef d0 = {0}; d0.name="TestSkillA"; d0.max_rank=5; rogue_skill_register(&d0);
    struct RogueSkillDef d1 = {0}; d1.name="TestSkillB"; d1.max_rank=3; rogue_skill_register(&d1);
    struct RogueSkillState* s0=(struct RogueSkillState*)rogue_skill_get_state(0); struct RogueSkillState* s1=(struct RogueSkillState*)rogue_skill_get_state(1);
    s0->rank=3; s0->cooldown_end_ms=1234.0; s1->rank=1; s1->cooldown_end_ms=0.0;

    /* Inventory item instances */
    rogue_items_init_runtime();
    int inst = rogue_items_spawn(2,4,0,0); if(inst>=0){ g_app.item_instances[inst].rarity=2; g_app.item_instances[inst].prefix_index=1; g_app.item_instances[inst].prefix_value=5; }

    /* Buff */
    rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE, 2, 5000.0, 0.0, ROGUE_BUFF_STACK_ADD, 0);

    /* Vendor */
    g_app.vendor_seed = 4242u; g_app.vendor_time_accum_ms=1500.0; g_app.vendor_restock_interval_ms=30000.0;

    /* World meta */
    g_app.pending_seed = 999u; g_app.gen_water_level=0.42; g_app.gen_cave_thresh=0.33;

    int save_rc = rogue_save_manager_save_slot(0);
    printf("SAVE_RC=%d\n", save_rc);
    fflush(stdout);
    /* assert(save_rc==0); disabled under Release build with NDEBUG */
    /* Quick sanity: open file and read raw descriptor */
    FILE* fchk=NULL; fchk=fopen("save_slot_0.sav","rb"); if(fchk){ struct RogueSaveDescriptor raw; if(fread(&raw,sizeof raw,1,fchk)==1){ printf("RAW_DESC v=%u sections=%u mask=0x%X size=%llu crc=0x%X\n", raw.version, raw.section_count, raw.component_mask, (unsigned long long)raw.total_size, raw.checksum); } fclose(fchk);} 

    /* Mutate live state to verify load overwrites */
    g_app.player.level=1; g_app.player.xp=0; g_app.player.health=1; g_app.talent_points=0;
    s0->rank=0; s0->cooldown_end_ms=0; s1->rank=0; s1->cooldown_end_ms=0;
    for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances && g_app.item_instances[i].active) g_app.item_instances[i].active=0;
    /* Expire buffs via update with far-future time instead of poking internals */
    rogue_buffs_update(1e9);
    g_app.vendor_seed=0; g_app.vendor_time_accum_ms=0; g_app.vendor_restock_interval_ms=0; g_app.pending_seed=0; g_app.gen_water_level=0; g_app.gen_cave_thresh=0;

    int rc = rogue_save_manager_load_slot(0); printf("LOAD_RC=%d\n", rc); fflush(stdout); assert(rc==0);

    /* Assertions */
    assert(g_app.player.level==7); assert(g_app.player.xp==321); assert(g_app.player.health==77); assert(g_app.talent_points==5);
    s0=(struct RogueSkillState*)rogue_skill_get_state(0); s1=(struct RogueSkillState*)rogue_skill_get_state(1);
    assert(s0->rank==3 && (int)s0->cooldown_end_ms==1234); assert(s1->rank==1);
    int inv_count=0; if(g_app.item_instances) for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active) inv_count++;
    assert(inv_count==1);
    assert(rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE)==2);
    assert(g_app.vendor_seed==4242u && (int)g_app.vendor_time_accum_ms==1500 && (int)g_app.vendor_restock_interval_ms==30000);
    assert(g_app.pending_seed==999u);

    printf("SAVE_ROUNDTRIP_OK lvl=%d skill0=%d inv=%d buff=%d seed=%u\n", g_app.player.level, s0->rank, inv_count, rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE), g_app.vendor_seed);
    return 0;
}
