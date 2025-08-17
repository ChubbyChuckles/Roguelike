#include "core/loot_multiplayer.h"
#include "core/loot_instances.h"
#include "core/app_state.h"
#include <stdlib.h>

static RogueLootMode g_mode = ROGUE_LOOT_MODE_SHARED;

void rogue_loot_set_mode(RogueLootMode m){ g_mode = m; }
RogueLootMode rogue_loot_get_mode(void){ return g_mode; }

int rogue_loot_assign_owner(int inst_index, int player_id){ if(inst_index<0||inst_index>=g_app.item_instance_cap) return -1; RogueItemInstance* it=&g_app.item_instances[inst_index]; if(!it->active) return -2; it->owner_player_id = (g_mode==ROGUE_LOOT_MODE_PERSONAL)? player_id : -1; return 0; }

int rogue_loot_need_greed_roll(int inst_index, int need_flag){ if(inst_index<0||inst_index>=g_app.item_instance_cap) return -1; const RogueItemInstance* it=&g_app.item_instances[inst_index]; if(!it->active) return -2; /* simple deterministic-ish roll placeholder */ int base = (need_flag?700:400); return base + rand()%300; }

int rogue_loot_trade_request(int inst_index, int from_player, int to_player){ if(inst_index<0||inst_index>=g_app.item_instance_cap) return -1; RogueItemInstance* it=&g_app.item_instances[inst_index]; if(!it->active) return -2; if(it->owner_player_id>=0 && it->owner_player_id!=from_player) return -3; /* not owned by requester */ it->owner_player_id = to_player; return 0; }
