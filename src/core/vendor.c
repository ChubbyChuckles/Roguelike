#include "core/vendor.h"
#include "core/loot_item_defs.h"
#include "core/loot_generation.h"
#include "core/loot_tables.h"
#include "core/loot_rarity.h"
#include "core/loot_drop_rates.h"
#include <string.h>

static RogueVendorItem g_vendor_items[ROGUE_VENDOR_SLOT_CAP];
static int g_vendor_count = 0;

void rogue_vendor_reset(void){ g_vendor_count = 0; }
int rogue_vendor_item_count(void){ return g_vendor_count; }
const RogueVendorItem* rogue_vendor_get(int index){ if(index<0||index>=g_vendor_count) return NULL; return &g_vendor_items[index]; }
int rogue_vendor_append(int def_index, int rarity, int price){ if(g_vendor_count>=ROGUE_VENDOR_SLOT_CAP) return -1; RogueVendorItem v; v.def_index=def_index; v.rarity=rarity; v.price=price; g_vendor_items[g_vendor_count++] = v; return g_vendor_count; }

/* Simple rarity -> price multiplier ladder (can be tuned later or data-driven). */
static int rarity_multiplier(int rarity){ static const int mult[5]={1,3,9,27,81}; if(rarity<0||rarity>4) return 1; return mult[rarity]; }

int rogue_vendor_price_formula(int item_def_index, int rarity){
    const RogueItemDef* idef = rogue_item_def_at(item_def_index); if(!idef) return 0;
    int base = idef->base_value>0? idef->base_value:1;
    int price = base * rarity_multiplier(rarity);
    /* Clamp to avoid overflow or zero */
    if(price < 1) price = 1; if(price > 100000000) price = 100000000;
    return price;
}

int rogue_vendor_generate_inventory(int loot_table_index, int slots, const RogueGenerationContext* ctx, unsigned int* rng_state){
    if(slots > ROGUE_VENDOR_SLOT_CAP) slots = ROGUE_VENDOR_SLOT_CAP;
    if(slots < 0) slots = 0;
    if(loot_table_index < 0 || !rng_state) return 0;
    /* Ensure drop rate scalars are initialized (static zero-initialization would suppress all drops). */
    rogue_drop_rates_reset();
    unsigned int local = *rng_state; int produced=0;
    for(int i=0;i<slots;i++){
        int attempts=0; int added=0;
        while(attempts<3 && !added){
            RogueGeneratedItem gi; memset(&gi,0,sizeof gi); gi.inst_index=-1;
            if(rogue_generate_item(loot_table_index, ctx, &local, &gi)==0){
                if(gi.def_index>=0){
                    RogueVendorItem v; v.def_index = gi.def_index; v.rarity = gi.rarity; v.price = rogue_vendor_price_formula(gi.def_index, gi.rarity>=0?gi.rarity:0);
                    if(g_vendor_count < ROGUE_VENDOR_SLOT_CAP){ g_vendor_items[g_vendor_count++] = v; produced++; added=1; }
                }
            }
            attempts++;
        }
    }
    *rng_state = local;
    return produced;
}

void rogue_vendor_rotation_init(RogueVendorRotation* rot, float interval_ms){
    if(!rot) return; memset(rot,0,sizeof *rot); rot->restock_interval_ms = interval_ms>0?interval_ms:60000.0f; rot->current_table=-1; }
int rogue_vendor_rotation_add_table(RogueVendorRotation* rot, int loot_table_index){ if(!rot) return -1; if(loot_table_index<0) return -1; if(rot->table_count>= (int)(sizeof rot->loot_table_indices/sizeof rot->loot_table_indices[0])) return -1; rot->loot_table_indices[rot->table_count++] = loot_table_index; if(rot->current_table<0) rot->current_table=0; return rot->table_count; }
int rogue_vendor_current_table(const RogueVendorRotation* rot){ if(!rot||rot->current_table<0||rot->current_table>=rot->table_count) return -1; return rot->loot_table_indices[rot->current_table]; }
int rogue_vendor_update_and_maybe_restock(RogueVendorRotation* rot, float dt_ms, const RogueGenerationContext* ctx, unsigned int* seed, int slots){
    if(!rot||!seed) return 0; if(rot->table_count==0) return 0; rot->time_accum_ms += dt_ms; if(rot->time_accum_ms < rot->restock_interval_ms) return 0; rot->time_accum_ms -= rot->restock_interval_ms; /* rotate table */
    rot->current_table = (rot->current_table+1) % rot->table_count; int table = rogue_vendor_current_table(rot); rogue_vendor_reset(); int produced = rogue_vendor_generate_inventory(table, slots, ctx, seed); return produced>0 ? 1:0; }
