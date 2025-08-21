#define SDL_MAIN_HANDLED
#include "game/combat.h"
#include <assert.h>
#include <stdio.h>

/* Stubs */
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
extern int rogue_force_attack_active; /* from combat */

/* Provide deterministic pseudo-rand via macro so we don't collide with CRT rand symbol (avoid
 * crits). */
static unsigned long rng_state = 1;
static int test_next_rand(void)
{
    rng_state = rng_state * 1103515245 + 12345;
    return (int) ((rng_state / 65536) % 32768);
}
#define rand test_next_rand
int main()
{
    RoguePlayer player;
    rogue_player_init(&player);
    player.strength = 40;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    player.facing = 2;
    player.dexterity = -100; /* force crit chance below zero */
    RoguePlayerCombat combat;
    rogue_combat_init(&combat);
    combat.phase = ROGUE_ATTACK_STRIKE;
    rogue_force_attack_active = 1;
    /* Reconstruct reach for frame 3 similar to combat.c to guarantee placement */
    float reach_curve_val = 1.35f; /* frame 3 */
    float base_reach = 1.6f * reach_curve_val;
    float reach = base_reach + (player.strength * 0.012f);
    float cx = player.base.pos.x + 1.0f * reach * 0.45f; /* dirx=1 */
    float target_x = cx + reach * 0.30f;                 /* comfortably within circle */
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof enemy);
    enemy.alive = 1;
    enemy.team_id = 1;
    enemy.base.pos.x = target_x;
    enemy.base.pos.y = 0.05f;
    enemy.health = 1000;
    enemy.max_health = 1000; /* slight y to avoid perpendicular edge */
    printf("debug reach=%.3f cx=%.3f ex=%.3f\n", reach, cx, enemy.base.pos.x);

    int hp0 = enemy.health;
    rogue_combat_player_strike(&combat, &player, &enemy, 1);
    int dmg1 = hp0 - enemy.health;
    printf("base strike dmg=%d\n", dmg1);
    if (dmg1 <= 0)
    {
        printf("no base damage dmg1=%d (reach=%.2f ex=%.2f cx=%.2f)\n", dmg1, reach,
               enemy.base.pos.x, cx);
        return 1;
    }
    int last = dmg1;
    for (int c = 0; c < 4; c++)
    {
        enemy.health = enemy.max_health; /* reset for isolated measurement */
        enemy.base.pos.x = target_x;
        enemy.base.pos.y = 0.05f; /* reset position (undo knockback) */
        combat.phase = ROGUE_ATTACK_STRIKE;
        combat.combo = c + 1;
        rogue_combat_player_strike(&combat, &player, &enemy, 1);
        int dealt = enemy.max_health - enemy.health;
        printf("combo=%d dealt=%d\n", c + 1, dealt);
        if (dealt <= last)
        {
            printf("expected strictly increasing scaling: prev=%d dealt=%d combo=%d\n", last, dealt,
                   c + 1);
            return 1;
        }
        last = dealt;
    }
    printf("combo scaling test ok base=%d final_combo=%d\n", dmg1, last);
    return 0;
}
