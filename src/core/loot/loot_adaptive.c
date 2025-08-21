#include "core/loot/loot_adaptive.h"
#include "core/loot/loot_item_defs.h"
#include <string.h>

/* Simple adaptive model:
   - Track counts per rarity & per category over a sliding window period (aggregate since reset for now).
   - Compute desired uniform target = average count.
   - Factor = clamp( target / max(actual,1), min_boost, max_boost ) with smoothing.
*/

static unsigned int g_rarity_counts[5];
static unsigned int g_category_counts[ROGUE_ITEM__COUNT];
static float g_rarity_factors[5];
static float g_category_factors[ROGUE_ITEM__COUNT];
/* Player preference (9.3): track pickups per category to bias future weights */
static unsigned int g_category_pickups[ROGUE_ITEM__COUNT];
static float g_category_preference_factors[ROGUE_ITEM__COUNT];

static float lerp(float a,float b,float t){ return a + (b-a)*t; }

void rogue_adaptive_reset(void){ memset(g_rarity_counts,0,sizeof g_rarity_counts); memset(g_category_counts,0,sizeof g_category_counts); memset(g_category_pickups,0,sizeof g_category_pickups); for(int i=0;i<5;i++){ g_rarity_factors[i]=1.0f; } for(int i=0;i<ROGUE_ITEM__COUNT;i++){ g_category_factors[i]=1.0f; g_category_preference_factors[i]=1.0f;} }

void rogue_adaptive_record_item(int item_def_index){ if(item_def_index<0) return; const RogueItemDef* d = rogue_item_def_at(item_def_index); if(!d) return; if(d->rarity>=0 && d->rarity<5) g_rarity_counts[d->rarity]++; if(d->category>=0 && d->category<ROGUE_ITEM__COUNT) g_category_counts[d->category]++; }
/* Record that player picked up an item (preference learning). */
void rogue_adaptive_record_pickup(int item_def_index){ if(item_def_index<0) return; const RogueItemDef* d = rogue_item_def_at(item_def_index); if(!d) return; if(d->category>=0 && d->category<ROGUE_ITEM__COUNT) g_category_pickups[d->category]++; }

void rogue_adaptive_recompute(void){
    /* Rarity factors */
    unsigned int total_rarity=0; for(int r=0;r<5;r++) total_rarity += g_rarity_counts[r];
    float target_r = (total_rarity>0)? (float)total_rarity / 5.0f : 0.0f;
    for(int r=0;r<5;r++){
        unsigned int c = g_rarity_counts[r]; float desired = target_r > 0? target_r : 0.0f; float raw = (c>0 && desired>0)? (desired / (float)c) : (desired>0? 1.5f:1.0f);
        if(raw < 0.5f) raw = 0.5f; if(raw > 2.0f) raw = 2.0f; /* clamp */
        /* Smooth toward new value */
        g_rarity_factors[r] = lerp(g_rarity_factors[r], raw, 0.25f);
    }
    /* Category factors */
    unsigned int total_cat=0; for(int c=0;c<ROGUE_ITEM__COUNT;c++) total_cat += g_category_counts[c];
    float target_c = (total_cat>0)? (float)total_cat / (float)ROGUE_ITEM__COUNT : 0.0f;
    for(int c=0;c<ROGUE_ITEM__COUNT;c++){
        unsigned int cc = g_category_counts[c]; float desired = target_c>0? target_c:0.0f; float raw = (cc>0 && desired>0)? (desired / (float)cc) : (desired>0? 1.5f:1.0f);
        if(raw < 0.5f) raw = 0.5f; if(raw > 2.0f) raw = 2.0f;
        g_category_factors[c] = lerp(g_category_factors[c], raw, 0.25f);
    }
    /* Preference factors: more pickups => slightly lower factor to encourage diversity, but clamp gently (0.75x - 1.25x) */
    unsigned int total_pick=0; for(int c=0;c<ROGUE_ITEM__COUNT;c++) total_pick += g_category_pickups[c];
    float avg_pick = (total_pick>0)? (float)total_pick / (float)ROGUE_ITEM__COUNT : 0.0f;
    for(int c=0;c<ROGUE_ITEM__COUNT;c++){
        unsigned int pc = g_category_pickups[c]; float base=1.0f;
        if(avg_pick>0 && pc>0){ float ratio = (float)pc / avg_pick; base = 1.0f / ratio; }
        if(base < 0.75f) base = 0.75f; if(base > 1.25f) base = 1.25f;
        g_category_preference_factors[c] = lerp(g_category_preference_factors[c], base, 0.25f);
    }
}

float rogue_adaptive_get_rarity_factor(int rarity){ if(rarity<0||rarity>4) return 1.0f; return g_rarity_factors[rarity]; }
float rogue_adaptive_get_category_factor(int category){ if(category<0||category>=ROGUE_ITEM__COUNT) return 1.0f; return g_category_factors[category]; }
float rogue_adaptive_get_category_preference_factor(int category){ if(category<0||category>=ROGUE_ITEM__COUNT) return 1.0f; return g_category_preference_factors[category]; }
