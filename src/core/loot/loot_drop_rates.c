#include "core/loot/loot_drop_rates.h"
#include "core/loot/loot_item_defs.h"

static float g_global_scalar = 1.0f;
static float g_category_scalar[ROGUE_ITEM__COUNT];
static int g_drop_rates_inited = 0;

static void ensure_init(void){
    if(!g_drop_rates_inited){
        for(int i=0;i<ROGUE_ITEM__COUNT;i++) g_category_scalar[i]=1.0f;
        g_drop_rates_inited = 1;
    }
}

void rogue_drop_rates_reset(void){
    g_global_scalar = 1.0f;
    for(int i=0;i<ROGUE_ITEM__COUNT;i++) g_category_scalar[i]=1.0f;
    g_drop_rates_inited = 1;
}

void rogue_drop_rates_set_global(float scalar){ ensure_init(); if(scalar < 0.0f) scalar = 0.0f; g_global_scalar = scalar; }
float rogue_drop_rates_get_global(void){ ensure_init(); return g_global_scalar; }

void rogue_drop_rates_set_category(int category, float scalar){ ensure_init(); if(category<0||category>=ROGUE_ITEM__COUNT) return; if(scalar<0.0f) scalar=0.0f; g_category_scalar[category]=scalar; }
float rogue_drop_rates_get_category(int category){ ensure_init(); if(category<0||category>=ROGUE_ITEM__COUNT) return 1.0f; return g_category_scalar[category]; }
