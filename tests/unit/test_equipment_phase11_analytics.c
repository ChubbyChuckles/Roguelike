/* Phase 11.1-11.5: Analytics snapshot, histograms, outlier flag */
#define SDL_MAIN_HANDLED 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/stat_cache.h"
#include "core/loot_instances.h"
#include "core/loot_item_defs.h"
#include "core/equipment.h"

int rogue_minimap_ping_loot(float x,float y,int r){(void)x;(void)y;(void)r;return 0;}
/* Stubs for required access */
static RogueItemDef defs[2];
const RogueItemDef* rogue_item_def_at(int idx){ if(idx>=0 && idx<2) return &defs[idx]; return NULL; }
int rogue_item_def_index(const char* id){ (void)id; return -1; }

/* Minimal equip implementation stubs */
static int equipped[ROGUE_EQUIP_SLOT_COUNT];
void rogue_equip_reset(void){ for(int i=0;i<ROGUE_EQUIP_SLOT_COUNT;i++) equipped[i]=-1; }
int rogue_equip_get(enum RogueEquipSlot slot){ return (slot>=0 && slot<ROGUE_EQUIP_SLOT_COUNT)? equipped[slot] : -1; }
int rogue_equip_try(enum RogueEquipSlot slot, int inst_index){ if(slot<0||slot>=ROGUE_EQUIP_SLOT_COUNT) return -1; equipped[slot]=inst_index; return 0; }
int rogue_equip_item_is_two_handed(int inst_index){ (void)inst_index; return 0; }
int rogue_equip_unequip(enum RogueEquipSlot slot){ int prev=equipped[slot]; equipped[slot]=-1; return prev; }
int rogue_equip_set_transmog(enum RogueEquipSlot slot,int def){(void)slot;(void)def;return 0;} int rogue_equip_get_transmog(enum RogueEquipSlot s){(void)s;return -1;}

int main(void){
    defs[0].category=ROGUE_ITEM_WEAPON; defs[0].base_damage_min=5; defs[0].base_damage_max=10; defs[0].rarity=2; defs[0].stack_max=1;
    defs[1].category=ROGUE_ITEM_ARMOR; defs[1].base_armor=15; defs[1].rarity=1; defs[1].stack_max=1;
    rogue_items_init_runtime(); rogue_equip_reset();
    int w = rogue_items_spawn(0,1,0,0); assert(w>=0); int a = rogue_items_spawn(1,1,0,0); assert(a>=0);
    rogue_equip_try(ROGUE_EQUIP_WEAPON,w); rogue_equip_try(ROGUE_EQUIP_ARMOR_CHEST,a);
    /* Simulate stat cache population (simplified) */
    g_player_stat_cache.dps_estimate = 100; g_player_stat_cache.ehp_estimate = 500; g_player_stat_cache.mobility_index=75;
    g_player_stat_cache.total_strength=10; g_player_stat_cache.total_dexterity=8; g_player_stat_cache.total_vitality=12; g_player_stat_cache.total_intelligence=5;

    char buf[256]; int n = rogue_equipment_stats_export_json(buf,sizeof buf); assert(n>0); assert(strstr(buf,"\"dps\":100"));
    for(int i=0;i<20;i++){ g_player_stat_cache.dps_estimate = 90 + (i%5); g_player_stat_cache.ehp_estimate = 480 + i; rogue_equipment_histogram_record(); }
    char hbuf[512]; int hn = rogue_equipment_histograms_export_json(hbuf,sizeof hbuf); assert(hn>0); assert(strstr(hbuf,"r2_s0"));
    /* Usage tracking (set/unique) -- definitions here have no set/unique so expect empty or '{}' */
    rogue_equipment_usage_record(); char ubuf[128]; int un = rogue_equipment_usage_export_json(ubuf,sizeof ubuf); assert(un>0); /* may be empty object */
    /* Outlier detection: feed consistent values then a large spike */
    g_player_stat_cache.dps_estimate = 95; for(int i=0;i<40;i++){ rogue_equipment_histogram_record(); }
    g_player_stat_cache.dps_estimate = 4000; int flag = rogue_equipment_dps_outlier_flag(); /* not recorded yet; need record */
    rogue_equipment_histogram_record(); flag = rogue_equipment_dps_outlier_flag(); assert(flag==1);
    printf("equipment_phase11_analytics_ok\n");
    return 0;
}
