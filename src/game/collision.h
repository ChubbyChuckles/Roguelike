#ifndef ROGUE_CORE_COLLISION_H
#define ROGUE_CORE_COLLISION_H
/* Generic collision helpers between gameplay actors (player, enemies, vegetation). */
struct RogueEnemy; /* fwd */
void rogue_collision_resolve_enemy_player(
    struct RogueEnemy* e); /* Push enemy out of player radius. */
#endif                     /* ROGUE_CORE_COLLISION_H */
