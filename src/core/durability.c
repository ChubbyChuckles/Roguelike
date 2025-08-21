#include "core/durability.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include <math.h>

/* Internal state for Phase 8.3 auto-warn bucket transitions. */
static int g_last_durability_transition = 0; /* 0 none,1 warn,2 critical */

void rogue_durability_notify_tick(void){ g_last_durability_transition = 0; /* reset each tick call; caller decides cadence */ }
int rogue_durability_last_transition(void){ int v = g_last_durability_transition; g_last_durability_transition = 0; return v; }

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
  /* Apply & transition detection */
  int before_cur=0, before_max=0; rogue_item_instance_get_durability(inst_index,&before_cur,&before_max);
  float before_pct = (before_max>0)? (float)before_cur/(float)before_max : 0.0f;
  rogue_item_instance_damage_durability(inst_index, loss);
  int after_cur=0, after_max=0; rogue_item_instance_get_durability(inst_index,&after_cur,&after_max);
  float after_pct = (after_max>0)? (float)after_cur/(float)after_max : 0.0f;
  int before_bucket = rogue_durability_bucket(before_pct);
  int after_bucket = rogue_durability_bucket(after_pct);
  if(after_bucket < before_bucket){
    if(after_bucket==1) g_last_durability_transition = 1; else if(after_bucket==0) g_last_durability_transition = 2; }
    return loss;
}
