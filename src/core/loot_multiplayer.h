#ifndef ROGUE_LOOT_MULTIPLAYER_H
#define ROGUE_LOOT_MULTIPLAYER_H
/* Phase 16.x multiplayer / personal loot scaffolding */
#ifdef __cplusplus
extern "C" { 
#endif

/* Modes */
typedef enum RogueLootMode { ROGUE_LOOT_MODE_SHARED=0, ROGUE_LOOT_MODE_PERSONAL=1 } RogueLootMode;

void rogue_loot_set_mode(RogueLootMode m);
RogueLootMode rogue_loot_get_mode(void);
int rogue_loot_assign_owner(int inst_index, int player_id); /* set owner for personal loot */
int rogue_loot_need_greed_roll(int inst_index, int need_flag); /* returns roll result (0-999) */
int rogue_loot_trade_request(int inst_index, int from_player, int to_player); /* stub validation */

#ifdef __cplusplus
}
#endif
#endif
