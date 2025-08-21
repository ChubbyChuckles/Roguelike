#ifndef ROGUE_LOOT_MULTIPLAYER_H
#define ROGUE_LOOT_MULTIPLAYER_H
/* Phase 16.x multiplayer / personal loot scaffolding */
#ifdef __cplusplus
extern "C"
{
#endif

    /* Modes */
    typedef enum RogueLootMode
    {
        ROGUE_LOOT_MODE_SHARED = 0,
        ROGUE_LOOT_MODE_PERSONAL = 1
    } RogueLootMode;

    void rogue_loot_set_mode(RogueLootMode m);
    RogueLootMode rogue_loot_get_mode(void);
    int rogue_loot_assign_owner(int inst_index, int player_id); /* set owner (or clear with -1) */

    /* Need / Greed roll system (Phase 16.3 full implementation) */
    /* Begin a need/greed session for an active instance. player_ids length = count (<=8). Returns 0
     * or error (<0). */
    int rogue_loot_need_greed_begin(int inst_index, const int* player_ids, int count);
    /* Submit a choice for a participant: need_flag=1 => NEED, need_flag=0 => GREED, pass_flag=1 =>
     * PASS. Returns roll (0-999) or code. */
    int rogue_loot_need_greed_choose(int inst_index, int player_id, int need_flag, int pass_flag);
    /* Resolve the session if all participants have chosen (or force). Returns winner player id or
     * -1 if unresolved/error. Assigns ownership. */
    int rogue_loot_need_greed_resolve(int inst_index);
    /* Query winner (after resolve). Returns player id or -1. */
    int rogue_loot_need_greed_winner(int inst_index);

    /* Trade validation (Phase 16.4). Returns 0 on success, <0 on error. */
    int rogue_loot_trade_request(int inst_index, int from_player, int to_player);

    /* Anti-dup safeguards: returns 1 if instance is locked (active need/greed session not yet
     * resolved). */
    int rogue_loot_instance_locked(int inst_index);

#ifdef __cplusplus
}
#endif
#endif
