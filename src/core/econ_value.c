#include "core/econ_value.h"

static float base_slot_factor_for_category(int cat){
    switch(cat){
        case ROGUE_ITEM_WEAPON: return 1.40f;
        case ROGUE_ITEM_ARMOR: return 1.30f;
        case ROGUE_ITEM_GEM: return 1.20f;
        case ROGUE_ITEM_CONSUMABLE: return 0.80f;
        case ROGUE_ITEM_MATERIAL: return 1.00f;
        case ROGUE_ITEM_MISC: return 0.90f;
        default: return 1.00f;
    }
}

int rogue_econ_rarity_multiplier(int rarity){ static const int mult[5]={1,3,9,27,81}; if(rarity<0||rarity>4) return 1; return mult[rarity]; }

int rogue_econ_item_value(int def_index, int rarity, int affix_power_raw, float durability_fraction){
    const RogueItemDef* d = rogue_item_def_at(def_index); if(!d) return 0;
    if(rarity<0) rarity=0; if(rarity>4) rarity=4;
    if(affix_power_raw < 0) affix_power_raw = 0;
    if(durability_fraction < 0.0f) durability_fraction = 0.0f; if(durability_fraction > 1.0f) durability_fraction = 1.0f;
    int base_value = d->base_value>0? d->base_value:1;
    float slot_factor = base_slot_factor_for_category(d->category);
    int rarity_mult = rogue_econ_rarity_multiplier(rarity);
    float norm_affix = 0.0f; if(affix_power_raw>0){ norm_affix = (float)affix_power_raw / ((float)affix_power_raw + 20.0f); }
    float durability_scalar = 0.4f + 0.6f * durability_fraction;
    double value = (double)base_value * slot_factor * (double)rarity_mult * (1.0 + norm_affix) * durability_scalar;
    if(value < 1.0) value = 1.0; if(value > 100000000.0) value = 100000000.0;
    int ivalue = (int)(value + 0.5);
    if(ivalue<1) ivalue=1; if(ivalue>100000000) ivalue=100000000;
    return ivalue;
}
