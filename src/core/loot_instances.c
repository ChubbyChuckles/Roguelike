#include "core/loot_instances.h"
#include "core/app_state.h"
#include <string.h>
#include "core/loot_logging.h"
#include "core/loot_affixes.h"

static RogueItemInstance g_instances[ROGUE_ITEM_INSTANCE_CAP];

void rogue_items_init_runtime(void){ memset(g_instances,0,sizeof g_instances); g_app.item_instances = g_instances; g_app.item_instance_cap = ROGUE_ITEM_INSTANCE_CAP; g_app.item_instance_count = 0; }
void rogue_items_shutdown_runtime(void){ g_app.item_instances=NULL; g_app.item_instance_cap=0; g_app.item_instance_count=0; }

int rogue_items_spawn(int def_index, int quantity, float x, float y){
    if(def_index<0 || quantity<=0){ ROGUE_LOOT_LOG_DEBUG("loot_spawn: rejected def=%d qty=%d", def_index, quantity); return -1; }
    for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++) if(!g_instances[i].active){
        const RogueItemDef* idef = rogue_item_def_at(def_index);
    int rarity = (idef? idef->rarity : 0);
    g_instances[i].def_index = def_index; g_instances[i].quantity = quantity; g_instances[i].x = x; g_instances[i].y = y; g_instances[i].life_ms=0; g_instances[i].active=1; g_instances[i].rarity=rarity;
    g_instances[i].prefix_index = -1; g_instances[i].suffix_index = -1; g_instances[i].prefix_value=0; g_instances[i].suffix_value=0;
        if(i >= g_app.item_instance_count) g_app.item_instance_count = i+1;
    ROGUE_LOOT_LOG_INFO("loot_spawn: def=%d qty=%d at(%.2f,%.2f) slot=%d active_total=%d", def_index, quantity, x, y, i, rogue_items_active_count()+1);
        return i; }
    ROGUE_LOG_WARN("loot_spawn: pool full (cap=%d) def=%d qty=%d", ROGUE_ITEM_INSTANCE_CAP, def_index, quantity);
    return -1;
}

const RogueItemInstance* rogue_item_instance_at(int index){ if(index<0 || index>=ROGUE_ITEM_INSTANCE_CAP) return NULL; if(!g_instances[index].active) return NULL; return &g_instances[index]; }

int rogue_item_instance_generate_affixes(int inst_index, unsigned int* rng_state, int rarity){
    if(inst_index<0 || inst_index>=ROGUE_ITEM_INSTANCE_CAP) return -1; if(!rng_state) return -1;
    RogueItemInstance* it = &g_instances[inst_index]; if(!it->active) return -1;
    /* Simple rule: rarity >=2 gives 1 prefix OR suffix (50/50), rarity >=3 gives both if possible */
    int want_prefix = 0, want_suffix = 0;
    if(rarity >= 2){ if(rarity >=3){ want_prefix=1; want_suffix=1; } else { want_prefix = ((*rng_state)&1)==0; want_suffix = !want_prefix; } }
    if(want_prefix){ int pi = rogue_affix_roll(ROGUE_AFFIX_PREFIX, rarity, rng_state); if(pi>=0){ it->prefix_index = pi; it->prefix_value = rogue_affix_roll_value(pi, rng_state); }}
    if(want_suffix){ int si = rogue_affix_roll(ROGUE_AFFIX_SUFFIX, rarity, rng_state); if(si>=0){ it->suffix_index = si; it->suffix_value = rogue_affix_roll_value(si, rng_state); }}
    return 0;
}

static int affix_damage_bonus(const RogueItemInstance* it){
    int bonus=0; if(!it) return 0;
    if(it->prefix_index>=0){ const RogueAffixDef* a = rogue_affix_at(it->prefix_index); if(a && a->stat==ROGUE_AFFIX_STAT_DAMAGE_FLAT) bonus += it->prefix_value; }
    if(it->suffix_index>=0){ const RogueAffixDef* a = rogue_affix_at(it->suffix_index); if(a && a->stat==ROGUE_AFFIX_STAT_DAMAGE_FLAT) bonus += it->suffix_value; }
    return bonus;
}

int rogue_item_instance_damage_min(int inst_index){ const RogueItemInstance* it = rogue_item_instance_at(inst_index); if(!it) return 0; const RogueItemDef* d = rogue_item_def_at(it->def_index); int base = d? d->base_damage_min:0; return base + affix_damage_bonus(it); }
int rogue_item_instance_damage_max(int inst_index){ const RogueItemInstance* it = rogue_item_instance_at(inst_index); if(!it) return 0; const RogueItemDef* d = rogue_item_def_at(it->def_index); int base = d? d->base_damage_max:0; return base + affix_damage_bonus(it); }

int rogue_item_instance_apply_affixes(int inst_index, int rarity, int prefix_index, int prefix_value, int suffix_index, int suffix_value){
    if(inst_index<0 || inst_index>=ROGUE_ITEM_INSTANCE_CAP) return -1; RogueItemInstance* it=&g_instances[inst_index]; if(!it->active) return -1;
    it->rarity = (rarity>=0 && rarity<=4)? rarity : it->rarity;
    it->prefix_index = prefix_index; it->prefix_value = prefix_value;
    it->suffix_index = suffix_index; it->suffix_value = suffix_value;
    return 0;
}

int rogue_items_active_count(void){ int c=0; for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++){ if(g_instances[i].active) c++; } return c; }
void rogue_items_update(float dt_ms){
    /* Advance lifetime & mark for despawn */
    for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++) if(g_instances[i].active){
        g_instances[i].life_ms += dt_ms;
        if(g_instances[i].life_ms >= (float)ROGUE_ITEM_DESPAWN_MS){
            g_instances[i].active=0; continue; }
    }
    /* Stack merge pass (single sweep O(n^2) small cap) */
    for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++) if(g_instances[i].active){
        for(int j=i+1;j<ROGUE_ITEM_INSTANCE_CAP;j++) if(g_instances[j].active){
            if(g_instances[i].def_index == g_instances[j].def_index && g_instances[i].rarity == g_instances[j].rarity){
                float dx = g_instances[i].x - g_instances[j].x; float dy = g_instances[i].y - g_instances[j].y;
                if(dx*dx + dy*dy <= ROGUE_ITEM_STACK_MERGE_RADIUS * ROGUE_ITEM_STACK_MERGE_RADIUS){
                    /* Merge j into i respecting stack_max */
                    const RogueItemDef* d = rogue_item_def_at(g_instances[i].def_index);
                    int stack_max = d? d->stack_max : 999999;
                    int space = stack_max - g_instances[i].quantity;
                    if(space>0){
                        int move = g_instances[j].quantity < space ? g_instances[j].quantity : space;
                        g_instances[i].quantity += move;
                        g_instances[j].quantity -= move;
                        if(g_instances[j].quantity <= 0){ g_instances[j].active = 0; }
                    }
                }
            }
        }
    }
}
