#include "core/durability.h"
#include "core/loot_instances.h"
#include "core/loot_item_defs.h"
#include <math.h>

/* Reuse existing classification already present in vendor_ui.c but keep centralized here for non-UI callers. */
int rogue_durability_bucket(float pct){ if(pct<0) pct=0; if(pct>1) pct=1; if(pct < 0.30f) return 0; if(pct < 0.60f) return 1; return 2; }

/* Non-linear durability decay model (Phase 8.1)
   We apply: loss = ceil( base * S * R ) where:
     S = log2(1 + severity/25)   (diminishing for large events)
     base = 1 (minimum chip) or elevated to 2 if severity >=50
     R = rarity factor = 1 / (1 + 0.35 * rarity)  (higher rarity loses less)
   A final floor ensures at least 1 lost when severity>0.
*/
int rogue_item_instance_apply_durability_event(int inst_index, int severity){
    if(severity <= 0) return 0;
    const RogueItemInstance* it = rogue_item_instance_at(inst_index); if(!it) return 0; if(it->durability_max <= 0) return 0;
    const RogueItemDef* def = rogue_item_def_at(it->def_index); if(!def) return 0;
    if(!(def->category==ROGUE_ITEM_WEAPON || def->category==ROGUE_ITEM_ARMOR)) return 0;
    int rarity = def->rarity; if(rarity<0) rarity=0; if(rarity>10) rarity=10;
    double S = log2(1.0 + (double)severity / 25.0);
    if(S < 0.2) S = 0.2; /* minimal scale */
    int base = (severity >= 50)? 2 : 1;
    double R = 1.0 / (1.0 + 0.35 * (double)rarity);
    double raw = (double)base * S * R;
    int loss = (int)ceil(raw);
    if(loss < 1) loss = 1;
    /* Apply */
    rogue_item_instance_damage_durability(inst_index, loss);
    return loss;
}
