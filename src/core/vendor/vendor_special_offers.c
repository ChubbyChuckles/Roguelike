/* Phase 6 Special Offers implementation */
#include "core/vendor/vendor_special_offers.h"
#include "core/loot/loot_item_defs.h"
#include "core/vendor/vendor_pricing.h"
#include "core/vendor/vendor_rng.h"
#include "core/crafting/material_registry.h"
#include <string.h>

static RogueVendorSpecialOffer g_offers[ROGUE_VENDOR_OFFER_SLOT_CAP];
static int g_offer_count=0;
static int g_consecutive_misses=0; /* for pity timer */
static unsigned int g_last_roll_seed=0;

/* Configuration */
static unsigned int offer_duration_ms(void){ return 10u*60u*1000u; } /* 10 min */
static int pity_threshold(void){ return 3; } /* guarantee after 3 miss cycles */

/* Lightweight xorshift */
static unsigned int xs32(unsigned int* s){ unsigned int x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }

void rogue_vendor_offers_reset(void){ memset(g_offers,0,sizeof g_offers); g_offer_count=0; g_consecutive_misses=0; g_last_roll_seed=0; }
int rogue_vendor_offers_count(void){ return g_offer_count; }
const RogueVendorSpecialOffer* rogue_vendor_offer_get(int index){ if(index<0||index>=g_offer_count) return NULL; return &g_offers[index]; }
int rogue_vendor_offers_pity_pending(void){ return g_consecutive_misses>=pity_threshold(); }

static void add_offer(unsigned int now_ms, int def_index, int rarity, int base_price, int nemesis_bonus, int scarcity_boost){ if(g_offer_count>=ROGUE_VENDOR_OFFER_SLOT_CAP) return; RogueVendorSpecialOffer o; o.def_index=def_index; o.rarity=rarity; o.base_price=base_price; o.expires_at_ms = now_ms + offer_duration_ms(); o.nemesis_bonus=nemesis_bonus; o.scarcity_boost=scarcity_boost; o.active=1; g_offers[g_offer_count++] = o; }

int rogue_vendor_offers_roll(unsigned int world_seed, unsigned int now_ms, int nemesis_defeated_flag){
    /* expire old offers */
    for(int i=0;i<g_offer_count;i++){ if(g_offers[i].active && now_ms >= g_offers[i].expires_at_ms) g_offers[i].active=0; }
    /* compact */
    int w=0; for(int i=0;i<g_offer_count;i++){ if(g_offers[i].active){ if(i!=w) g_offers[w]=g_offers[i]; w++; } } g_offer_count=w;
    /* Phase 12 governed offers RNG (vendor_id not yet differentiated, pass NULL) */
    unsigned int seed = rogue_vendor_seed_compose(world_seed ^ (nemesis_defeated_flag?0xDEADBEAFu:0x1234u), NULL, g_last_roll_seed, ROGUE_VENDOR_RNG_OFFERS);
    g_last_roll_seed = seed;
    int scarcity_material_index = -1; int scarcity_found=0;
    /* Scarcity heuristic: pick material with lowest base value (proxy for deficit) */
    int mat_count = rogue_material_count(); int lowest_val=999999;
    for(int m=0;m<mat_count;m++){ const RogueMaterialDef* me = rogue_material_get(m); if(!me) continue; if(me->base_value < lowest_val){ lowest_val = me->base_value; scarcity_material_index = me->item_def_index; scarcity_found=1; } }
    int guaranteed = (g_consecutive_misses>=pity_threshold());
    int produced_before = g_offer_count; int attempts=0;
    while(g_offer_count < ROGUE_VENDOR_OFFER_SLOT_CAP && attempts<32){
        attempts++;
        int pick_def=-1; int rarity=0; int via_scarcity=0; int via_nemesis=0;
        unsigned int r = xs32(&seed);
        if(nemesis_defeated_flag && (r & 3)==0){ /* 25% chance special blueprint if nemesis */
            /* blueprint heuristic: choose high rarity item def (last quarter of defs) */
            int total = rogue_item_defs_count(); if(total>0){ pick_def = (total*3)/4 + (int)(r % (total/4? total/4:1)); rarity=4; via_nemesis=1; }
        }
        if(pick_def<0 && scarcity_found && ((r>>2) & 3)==0){ pick_def = scarcity_material_index; rarity=1; via_scarcity=1; }
        if(pick_def<0){ int total = rogue_item_defs_count(); if(total==0) break; pick_def = (int)(r % total); const RogueItemDef* d=rogue_item_def_at(pick_def); if(d&&d->rarity>=0) rarity=d->rarity; }
        if(pick_def<0) break;
        /* Basic price via pricing engine with vendor context neutral */
        int base_price = rogue_vendor_compute_price(-1,pick_def,rarity,-1,1,100,-1,0.0f);
        add_offer(now_ms,pick_def,rarity,base_price,via_nemesis,via_scarcity);
    }
    if(g_offer_count==produced_before){ g_consecutive_misses++; } else { g_consecutive_misses=0; }
    if(guaranteed && g_offer_count==0){ /* guarantee at least one scarcity or nemesis item */
        if(scarcity_material_index>=0){ int base_price = rogue_vendor_compute_price(-1,scarcity_material_index,1,-1,1,100,-1,0.0f); add_offer(now_ms,scarcity_material_index,1,base_price,0,1); g_consecutive_misses=0; }
    }
    return g_offer_count - produced_before;
}
