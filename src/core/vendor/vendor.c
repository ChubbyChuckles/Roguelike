#include "core/vendor/vendor.h"
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_generation.h"
#include "core/loot/loot_tables.h"
#include "core/loot/loot_rarity.h"
#include "core/loot/loot_drop_rates.h"
#include <string.h>
#include "core/vendor/econ_value.h"
#include "core/vendor/vendor_registry.h"
#include "core/vendor/vendor_inventory_templates.h"
#include "core/crafting/crafting.h"
#include "core/vendor/vendor_pricing.h"
#include "core/vendor/vendor_adaptive.h"
#include "core/vendor/vendor_rng.h"
#include <stdio.h> /* debug diagnostics */

static RogueVendorItem g_vendor_items[ROGUE_VENDOR_SLOT_CAP];
static int g_vendor_count = 0;

void rogue_vendor_reset(void){ g_vendor_count = 0; }
int rogue_vendor_item_count(void){ return g_vendor_count; }
const RogueVendorItem* rogue_vendor_get(int index){ if(index<0||index>=g_vendor_count) return NULL; return &g_vendor_items[index]; }
int rogue_vendor_append(int def_index, int rarity, int price){ if(g_vendor_count>=ROGUE_VENDOR_SLOT_CAP) return -1; RogueVendorItem v; v.def_index=def_index; v.rarity=rarity; v.price=price; g_vendor_items[g_vendor_count++] = v; return g_vendor_count; }

/* Simple rarity -> price multiplier ladder (can be tuned later or data-driven). */
int rogue_vendor_price_formula(int item_def_index, int rarity){
    /* Delegate to pricing engine with neutral context (vendor selling, full condition, no rep/negotiation) */
    return rogue_vendor_compute_price(-1, item_def_index, rarity, -1, 1, 100, -1, 0.0f);
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

/* --- Phase 2.3–2.5 Constrained Template‑Driven Generation Implementation --- */
static unsigned int xorshift32_local(unsigned int* s){ unsigned int x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }
static int weighted_pick(const int* weights, int count, unsigned int* state){ int total=0; for(int i=0;i<count;i++){ if(weights[i]>0) total+=weights[i]; }
    if(total<=0) return -1; unsigned int r = xorshift32_local(state) % (unsigned int)total; int acc=0; for(int i=0;i<count;i++){ if(weights[i]<=0) continue; acc += weights[i]; if((int)r < acc) return i; } return -1; }

/* Clamp rarity respecting remaining caps */
static int rarity_from_weights_with_caps(const int* weights, int caps[5], unsigned int* state){
    int adj[5]; for(int i=0;i<5;i++){ adj[i] = (caps[i]==0)?0:weights[i]; if(adj[i]<0) adj[i]=0; }
    return weighted_pick(adj,5,state);
}

int rogue_vendor_generate_constrained(const char* vendor_id, unsigned int world_seed, int day_cycle, int slots){
    int rarity_caps[5]; int rarity_used[5]; int ensured_consumable; int ensured_material; int ensured_recipe; int used_defs[ROGUE_VENDOR_SLOT_CAP]; int used_count; int cat_lists[ROGUE_ITEM__COUNT][512]; int cat_counts[ROGUE_ITEM__COUNT]; int total_defs; unsigned int seed; int produced; int attempts_guard; int i;
    if(slots <=0) return 0; if(slots>ROGUE_VENDOR_SLOT_CAP) slots = ROGUE_VENDOR_SLOT_CAP;
    const RogueVendorDef* vd = rogue_vendor_def_find(vendor_id); if(!vd) return 0; const RogueVendorInventoryTemplate* tpl = rogue_vendor_inventory_template_find(vd->archetype); if(!tpl) return 0;
    /* Phase 12: use governed RNG stream for inventory */
    seed = rogue_vendor_seed_compose(world_seed, vendor_id, (uint32_t)day_cycle, ROGUE_VENDOR_RNG_INVENTORY);
    rogue_vendor_reset();
    rarity_caps[0]=slots; rarity_caps[1]=slots; rarity_caps[2]=4; rarity_caps[3]=2; rarity_caps[4]=1;
    for(i=0;i<5;i++) rarity_used[i]=0; ensured_consumable=0; ensured_material=0; ensured_recipe=0; used_count=0;
    memset(cat_counts,0,sizeof cat_counts);
    total_defs = rogue_item_defs_count();
    for(i=0;i<total_defs;i++){ const RogueItemDef* d = rogue_item_def_at(i); if(!d) continue; if(d->category<0||d->category>=ROGUE_ITEM__COUNT) continue; if(cat_counts[d->category] < 512){ cat_lists[d->category][cat_counts[d->category]++] = i; } }
    /* total_defs may be zero in atypical test harness situations; generation loop will then produce 0 */
    produced=0; attempts_guard = slots * 10;
    while(produced < slots && attempts_guard-- > 0){
        int rarity = rarity_from_weights_with_caps(tpl->rarity_weights, rarity_caps, &seed);
        int cat; int pick_index; const RogueItemDef* d;
        if(rarity<0) rarity=0; if(rarity>4) rarity=4;
        /* If template suggests a rarity whose cap is exhausted, fallback to next lower tier with availability */
        if(rarity_used[rarity] >= rarity_caps[rarity]){
            int rr = rarity; while(rr>0 && rarity_used[rr] >= rarity_caps[rr]) rr--; rarity = rr;
            if(rarity_used[rarity] >= rarity_caps[rarity]) continue; /* all tiers exhausted */
        }
    int adaptive_weights[ROGUE_ITEM__COUNT];
    rogue_vendor_adaptive_apply_category_weights(adaptive_weights, tpl->category_weights, ROGUE_ITEM__COUNT);
    cat = weighted_pick(adaptive_weights, ROGUE_ITEM__COUNT, &seed);
        if(cat<0) cat=ROGUE_ITEM_MISC; if(cat>=ROGUE_ITEM__COUNT) cat=ROGUE_ITEM_MISC;
        if(cat_counts[cat]==0) continue;
        pick_index = cat_lists[cat][ xorshift32_local(&seed) % cat_counts[cat] ];
        /* duplicate check */
        int dup=0; for(i=0;i<used_count;i++){ if(used_defs[i]==pick_index){ dup=1; break; } }
        if(dup) continue;
        d = rogue_item_def_at(pick_index); if(!d) continue;
        if(d->rarity>=0){
            int dr = d->rarity; if(dr<0) dr=0; if(dr>4) dr=4;
            /* enforce caps: if item intrinsic rarity exceeds remaining cap, downgrade to lowest available */
            if(rarity_used[dr] < rarity_caps[dr]) rarity = dr; else {
                int rr = dr; while(rr>0 && rarity_used[rr] >= rarity_caps[rr]) rr--; if(rarity_used[rr] < rarity_caps[rr]) rarity = rr; else continue; /* cannot place */
            }
        }
        { RogueVendorItem v; v.def_index=pick_index; v.rarity=rarity; v.price=rogue_vendor_price_formula(pick_index,rarity); g_vendor_items[g_vendor_count++]=v; }
        used_defs[used_count++]=pick_index; rarity_used[rarity]++; produced++;
        if(d->category==ROGUE_ITEM_CONSUMABLE) ensured_consumable=1; else if(d->category==ROGUE_ITEM_MATERIAL) ensured_material=1;
    }
    if(produced>0 && (!ensured_consumable || !ensured_material)){
        for(int c_try=0;c_try<ROGUE_ITEM__COUNT && (!ensured_consumable || !ensured_material); c_try++){
            for(i=0;i<total_defs && (!ensured_consumable || !ensured_material); i++){
                const RogueItemDef* d = rogue_item_def_at(i); int j; int dup=0; if(!d) continue; for(j=0;j<used_count;j++){ if(used_defs[j]==i){ dup=1; break; } } if(dup) continue;
                if(!ensured_consumable && d->category==ROGUE_ITEM_CONSUMABLE){ if(produced<slots){ RogueVendorItem v; v.def_index=i; v.rarity=d->rarity>=0?d->rarity:0; v.price=rogue_vendor_price_formula(i,v.rarity); g_vendor_items[g_vendor_count++]=v; used_defs[used_count++]=i; produced++; } else { g_vendor_items[produced-1].def_index=i; g_vendor_items[produced-1].rarity=d->rarity>=0?d->rarity:0; g_vendor_items[produced-1].price=rogue_vendor_price_formula(i,g_vendor_items[produced-1].rarity); } ensured_consumable=1; }
                if(!ensured_material && d->category==ROGUE_ITEM_MATERIAL){ if(produced<slots){ RogueVendorItem v; v.def_index=i; v.rarity=d->rarity>=0?d->rarity:0; v.price=rogue_vendor_price_formula(i,v.rarity); g_vendor_items[g_vendor_count++]=v; used_defs[used_count++]=i; produced++; } else { g_vendor_items[0].def_index=i; g_vendor_items[0].rarity=d->rarity>=0?d->rarity:0; g_vendor_items[0].price=rogue_vendor_price_formula(i,g_vendor_items[0].rarity); } ensured_material=1; }
            }
        }
    }
    if(produced < slots){
        int rcnt = rogue_craft_recipe_count();
        for(int r=0;r<rcnt && !ensured_recipe;r++){
            const RogueCraftRecipe* cr = rogue_craft_recipe_at(r); int di; const RogueItemDef* d; int j; int dup=0; if(!cr) continue; di = cr->output_def; if(di<0) continue; for(j=0;j<used_count;j++){ if(used_defs[j]==di){ dup=1; break; } } if(dup) continue; d = rogue_item_def_at(di); if(!d) continue; { RogueVendorItem v; int rarity = d->rarity>=0?d->rarity:0; v.def_index=di; v.rarity=rarity; v.price= rogue_vendor_price_formula(di,rarity); g_vendor_items[g_vendor_count++] = v; } used_defs[used_count++]=di; produced++; ensured_recipe=1; }
    }
    for(i=0;i<produced;i++){ int j; for(j=i+1;j<produced;j++){ if(g_vendor_items[j].def_index < g_vendor_items[i].def_index){ RogueVendorItem tmp=g_vendor_items[i]; g_vendor_items[i]=g_vendor_items[j]; g_vendor_items[j]=tmp; } } }
    return produced;
}

void rogue_vendor_rotation_init(RogueVendorRotation* rot, float interval_ms){
    if(!rot) return; memset(rot,0,sizeof *rot); rot->restock_interval_ms = interval_ms>0?interval_ms:60000.0f; rot->current_table=-1; }
int rogue_vendor_rotation_add_table(RogueVendorRotation* rot, int loot_table_index){ if(!rot) return -1; if(loot_table_index<0) return -1; if(rot->table_count>= (int)(sizeof rot->loot_table_indices/sizeof rot->loot_table_indices[0])) return -1; rot->loot_table_indices[rot->table_count++] = loot_table_index; if(rot->current_table<0) rot->current_table=0; return rot->table_count; }
int rogue_vendor_current_table(const RogueVendorRotation* rot){ if(!rot||rot->current_table<0||rot->current_table>=rot->table_count) return -1; return rot->loot_table_indices[rot->current_table]; }
int rogue_vendor_update_and_maybe_restock(RogueVendorRotation* rot, float dt_ms, const RogueGenerationContext* ctx, unsigned int* seed, int slots){
    if(!rot||!seed) return 0; if(rot->table_count==0) return 0; rot->time_accum_ms += dt_ms; if(rot->time_accum_ms < rot->restock_interval_ms) return 0; rot->time_accum_ms -= rot->restock_interval_ms; /* rotate table */
    rot->current_table = (rot->current_table+1) % rot->table_count; int table = rogue_vendor_current_table(rot); rogue_vendor_reset(); int produced = rogue_vendor_generate_inventory(table, slots, ctx, seed); return produced>0 ? 1:0; }
