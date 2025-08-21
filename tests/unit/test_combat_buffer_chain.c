#define SDL_MAIN_HANDLED
#include "game/combat.h"
#include <assert.h>
#include <stdio.h>

int test_local_get_current_attack_frame(void) { return 3; }
#define rogue_get_current_attack_frame test_local_get_current_attack_frame
RoguePlayer g_exposed_player_for_stats; /* unused */
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

/* We directly drive the update function to assert buffered chaining reduces idle gaps. */
int main()
{
    RoguePlayerCombat pc;
    rogue_combat_init(&pc);
    RoguePlayer player;
    rogue_player_init(&player);
    float dt = 30.0f; /* ms per tick */
    int presses = 0;
    int transitions = 0;
    int lastPhase = -1;
    for (int frame = 0; frame < 80; ++frame)
    {
        int press = 0;
        /* Press at start and near expected recover end to buffer next */
        if (frame == 0 || frame == 15 || frame == 30 || frame == 45 || frame == 60)
        {
            press = 1;
            presses++;
        }
        rogue_combat_update_player(&pc, dt, press);
        if (pc.phase != lastPhase)
        {
            transitions++;
            lastPhase = pc.phase;
        }
    }
    if (pc.combo < 2)
    {
        printf("expected combo to advance, combo=%d\n", pc.combo);
        return 1;
    }
    if (transitions < 5)
    {
        printf("too few transitions=%d (buffering failed)\n", transitions);
        return 1;
    }
    printf("buffer chain test ok combo=%d transitions=%d presses=%d\n", pc.combo, transitions,
           presses);
    return 0;
}
