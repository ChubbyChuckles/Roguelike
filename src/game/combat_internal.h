#ifndef ROGUE_GAME_COMBAT_INTERNAL_H
#define ROGUE_GAME_COMBAT_INTERNAL_H
/* Internal-only combat interfaces (not for public game-wide consumption).
   Keeps private hooks and helpers out of the main public header to reduce surface area. */

/* Forward private obstruction test hook access (defined in combat_hooks.c) */
int _rogue_combat_call_obstruction_test(float sx,float sy,float ex,float ey);
/* Internal query for hyper armor flag (defined in combat_player_state.c) */
int _rogue_player_is_hyper_armor_active(void);

#endif /* ROGUE_GAME_COMBAT_INTERNAL_H */
