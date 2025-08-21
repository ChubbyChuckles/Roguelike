#define SDL_MAIN_HANDLED
#include "game/combat.h"
#include <assert.h>
#include <stdio.h>

int test_local_get_current_attack_frame(void) { return 3; }
#define rogue_get_current_attack_frame test_local_get_current_attack_frame
RoguePlayer g_exposed_player_for_stats;
void test_local_add_damage_number(float x, float y, int amount, int from_player)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
}
void test_local_add_damage_number_ex(float x, float y, int amount, int from_player, int crit)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
    (void) crit;
}
#define rogue_add_damage_number test_local_add_damage_number
#define rogue_add_damage_number_ex test_local_add_damage_number_ex

/* Validate early cancel: when hit_confirmed and buffered attack, STRIKE should end earlier than
 * full STRIKE_MS (70ms baseline). */
int main()
{
    RoguePlayer player;
    rogue_player_init(&player);
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    player.facing = 2;
    player.strength = 10;
    RogueEnemy enemy;
    enemy.alive = 1;
    enemy.base.pos.x = 0.9f;
    enemy.base.pos.y = 0.0f;
    enemy.health = 50;
    enemy.max_health = 50;
    RoguePlayerCombat pc;
    rogue_combat_init(&pc);
    /* Start attack */
    rogue_combat_update_player(&pc, 0, 1); /* press -> windup */
    /* Advance to strike */
    for (int t = 0; t < 5; t++)
        rogue_combat_update_player(&pc, 25.0f, 0);
    assert(pc.phase == ROGUE_ATTACK_STRIKE);
    /* Perform strike (register hit) */
    rogue_combat_player_strike(&pc, &player, &enemy, 1);
    /* Buffer next attack mid-strike */
    rogue_combat_update_player(&pc, 20.0f, 1);
    /* After a short additional time (< full 70ms) strike should transition */
    rogue_combat_update_player(&pc, 20.0f, 0);
    if (pc.phase == ROGUE_ATTACK_STRIKE)
    {
        printf("early cancel did not transition (phase still STRIKE)\n");
        return 1;
    }
    printf("early cancel test ok combo=%d phase=%d\n", pc.combo, pc.phase);
    return 0;
}
