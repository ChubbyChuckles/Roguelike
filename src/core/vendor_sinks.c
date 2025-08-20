#include "core/vendor_sinks.h"
#include "core/material_registry.h"
#include "core/loot_instances.h"
#include "core/equipment_enchant.h" /* for affix reroll helper */
#include <string.h>

static int g_sink_totals[ROGUE_SINK__COUNT];

void rogue_vendor_sinks_reset(void){ memset(g_sink_totals,0,sizeof g_sink_totals); }
void rogue_vendor_sinks_add(int category, int amount){ if(category<0||category>=ROGUE_SINK__COUNT||amount<=0) return; long long v = (long long)g_sink_totals[category] + amount; if(v>2000000000LL) v=2000000000LL; g_sink_totals[category]=(int)v; }
int rogue_vendor_sinks_total(int category){ if(category<0||category>=ROGUE_SINK__COUNT) return 0; return g_sink_totals[category]; }
int rogue_vendor_sinks_grand_total(void){ long long s=0; for(int i=0;i<ROGUE_SINK__COUNT;i++){ s+=g_sink_totals[i]; } if(s>2000000000LL) s=2000000000LL; return (int)s; }

/* Fee scaling helper: base*(1 + level^0.5 * k), k=0.015 */
static int scale_fee(int base,int player_level){ if(base<=0) return 0; if(player_level<1) player_level=1; double mult = 1.0 + 0.015 * (double)(player_level>1? player_level:1); double v = (double)base * mult; long long r=(long long)(v+0.5); if(r>2000000000LL) r=2000000000LL; return (int)r; }

int rogue_vendor_upgrade_reroll_affix(int inst_index, int reroll_prefix, int reroll_suffix,
                                      int player_level, int catalyst_count_available,
                                      RogueConsumeCatalystFn consume_catalyst_cb,
                                      RogueSpendGoldFn spend_gold_cb,
                                      int* out_gold_fee){
    if(out_gold_fee) *out_gold_fee=0;
    if(inst_index<0 || (!reroll_prefix && !reroll_suffix) || !spend_gold_cb) return -1;
    if(reroll_prefix && reroll_suffix && catalyst_count_available<=0) return -3; /* require catalyst for dual */
    /* derive rarity & compute base fee heuristic */
    int rarity=0; int has_prefix=0, has_suffix=0;
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if(!it) return -1;
    rarity = it->rarity;
    has_prefix = it->prefix_index>=0; has_suffix = it->suffix_index>=0;
    if(reroll_prefix && !has_prefix && reroll_suffix && !has_suffix) return -2;
    if(reroll_prefix && !has_prefix) reroll_prefix=0; if(reroll_suffix && !has_suffix) reroll_suffix=0;
    if(!reroll_prefix && !reroll_suffix) return -2;
    int base_fee = 40 + rarity * 60; if(reroll_prefix && reroll_suffix) base_fee = (int)(base_fee * 1.75);
    int gold_fee = scale_fee(base_fee, player_level);
    if(spend_gold_cb(gold_fee)!=0) return -4; /* insufficient */
    if(reroll_prefix && reroll_suffix){ if(consume_catalyst_cb && consume_catalyst_cb()!=0) return -3; }
    int enchant_cost=0; int reroll_rc = rogue_item_instance_enchant(inst_index, reroll_prefix, reroll_suffix, &enchant_cost);
    if(reroll_rc<0) return -5;
    rogue_vendor_sinks_add(ROGUE_SINK_UPGRADE, gold_fee);
    if(out_gold_fee) *out_gold_fee=gold_fee;
    return 0;
}

int rogue_vendor_material_trade_in(int from_material_index, int to_material_index, int count_in,
                                   int player_level,
                                   RogueConsumeSourceMatsFn consume_source_cb,
                                   RogueGrantTargetMatsFn grant_target_cb,
                                   RogueSpendGoldFn spend_gold_cb,
                                   int* out_count_out, int* out_gold_fee){
    if(out_count_out) *out_count_out=0; if(out_gold_fee) *out_gold_fee=0;
    if(from_material_index<0||to_material_index<0||count_in<=0||!consume_source_cb||!grant_target_cb||!spend_gold_cb) return -1;
    const RogueMaterialDef* from = rogue_material_get(from_material_index);
    const RogueMaterialDef* to = rogue_material_get(to_material_index);
    if(!from||!to) return -1;
    int rate = 6; /* 6:1 unfavorable baseline */
    if(count_in < rate) return -2;
    int out = count_in / rate; if(out<=0) return -2;
    int base_fee = (from->base_value>0? from->base_value:10) * out; /* gold fee scales with source value */
    int gold_fee = scale_fee(base_fee, player_level);
    if(spend_gold_cb(gold_fee)!=0) return -4;
    if(consume_source_cb(count_in)!=0) return -3;
    if(grant_target_cb(out)!=0) return -5;
    rogue_vendor_sinks_add(ROGUE_SINK_TRADEIN, gold_fee);
    if(out_count_out) *out_count_out=out;
    if(out_gold_fee) *out_gold_fee=gold_fee;
    return 0;
}
