#include "core/vendor/economy.h"
#include <math.h> /* sqrt for dynamic pricing curves */
#include <string.h>

static int g_gold = 0;
static int g_reputation = 0; /* 0..100 */

void rogue_econ_reset(void){ g_gold=0; g_reputation=0; }
int rogue_econ_gold(void){ return g_gold; }
int rogue_econ_add_gold(int amount){ long long v = (long long)g_gold + amount; if(v<0) v=0; if(v>2000000000LL) v=2000000000LL; g_gold=(int)v; return g_gold; }
void rogue_econ_set_reputation(int rep){ if(rep<0) rep=0; if(rep>100) rep=100; g_reputation=rep; }
int rogue_econ_get_reputation(void){ return g_reputation; }

int rogue_econ_buy_price(const RogueVendorItem* v){ if(!v) return 0; int base=v->price; double disc = 1.0 - (double)g_reputation * 0.002; if(disc < 0.5) disc = 0.5; int p = (int)(base * disc + 0.5); if(p<1) p=1; return p; }
int rogue_econ_sell_value(const RogueVendorItem* v){ if(!v) return 0; int base=v->price; int val = base / 5; if(val<1) val=1; int cap = (base * 70)/100; if(val>cap) val=cap; return val; }

int rogue_econ_try_buy(const RogueVendorItem* v){ if(!v) return -1; int cost = rogue_econ_buy_price(v); if(g_gold < cost) return -2; rogue_econ_add_gold(-cost); return 0; }
int rogue_econ_sell(const RogueVendorItem* v){ if(!v) return 0; int credit = rogue_econ_sell_value(v); rogue_econ_add_gold(credit); return credit; }

/* Currency sink helpers (10.4) */
int rogue_econ_repair_cost_ex(int durability_missing, int rarity, int item_level){
	if(durability_missing <= 0) return 0;
	if(rarity < 0) rarity = 0; if(rarity > 10) rarity = 10;
	if(item_level < 1) item_level = 1; if(item_level > 999) item_level = 999;
	/* Base unit cost rises gently with rarity; item_level introduces quadratic soft curve via sqrt.
	   Formula: unit = (6 + rarity*6) * (1 + sqrt(item_level)/45)
	   Total cost = durability_missing * unit rounded. */
	double unit = (6.0 + (double)rarity * 6.0) * (1.0 + sqrt((double)item_level) / 45.0);
	double raw = unit * (double)durability_missing;
	long long cost = (long long)(raw + 0.5);
	if(cost > 2000000000LL) cost = 2000000000LL;
	return (int)cost;
}
/* Legacy wrapper preserves original linear Phase 10.4 semantics used by older tests:
   unit = 5 + rarity*5 (rarity clamped 0..10), total = durability_missing * unit.
   Extended API ( _ex ) introduced rarity*6 + sqrt(level) scaling, but tests like
   test_economy_sink still assert the legacy costs (50, 250 for missing=10 r0/r4).
   Keep wrapper independent so changes to the extended formula don't break legacy tests. */
int rogue_econ_repair_cost(int durability_missing, int rarity){
	if(durability_missing <= 0) return 0;
	if(rarity < 0) rarity = 0; if(rarity > 10) rarity = 10;
	int unit = 5 + rarity * 5;
	long long cost = (long long)durability_missing * unit;
	if(cost > 2000000000LL) cost = 2000000000LL;
	return (int)cost;
}

int rogue_econ_reroll_affix_cost(int rarity){
	if(rarity < 0) rarity = 0; if(rarity > 10) rarity = 10;
	int mult = 1 << rarity; if(mult > 1024) mult = 1024; /* clamp */
	long long cost = 50LL * mult;
	if(cost > 2000000000LL) cost = 2000000000LL;
	return (int)cost;
}
