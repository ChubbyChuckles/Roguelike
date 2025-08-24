#define SDL_MAIN_HANDLED
#include "../../src/entities/player.h"
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void) { return 3; }
void rogue_app_add_hitstop(float ms) { (void) ms; }
void rogue_add_damage_number(float x, float y, int amount, int from_player)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
}
void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
    (void) crit;
}

static void init_player(RoguePlayer* p)
{
    rogue_player_init(p);
    p->facing = 0; /* facing down */
}

int main()
{
    RoguePlayer p;
    init_player(&p);
    g_exposed_player_for_stats = p;
    /* Begin guard facing down */
    int ok = rogue_player_begin_guard(&p, 0);
    assert(ok == 1);
    int blocked = 0, perfect = 0;
    /* Incoming attack from front (up towards player is (0,-1) so enemy below player ->
     * attack_dir=(0,-1) giving dot with facing (0,1) = -1; we instead simulate enemy above player
     * attacking downward -> dir (0,1) */
    int dmg_full = 100;
    int applied =
        rogue_player_apply_incoming_melee(&p, (float) dmg_full, 0.0f, 1.0f, 0, &blocked, &perfect);
    assert(blocked == 1);
    assert(perfect == 1); /* within perfect window at t=0 */
    assert(applied == 0); /* perfect guard negates damage */
    /* Advance guard time beyond perfect window and block again */
    p.guard_active_time_ms = p.perfect_guard_window_ms + 10.0f;
    blocked = 0;
    perfect = 0;
    int applied2 =
        rogue_player_apply_incoming_melee(&p, (float) dmg_full, 0.0f, 1.0f, 0, &blocked, &perfect);
    assert(blocked == 1 && perfect == 0);
    assert(applied2 >= (int) (dmg_full * ROGUE_GUARD_CHIP_PCT) - 1); /* chip damage */
    /* Attack from behind should not be blocked */
    blocked = 0;
    perfect = 0;
    int applied3 =
        rogue_player_apply_incoming_melee(&p, (float) dmg_full, 0.0f, -1.0f, 0, &blocked, &perfect);
    assert(blocked == 0 && applied3 == dmg_full);
    printf("phase3_guard_block_basic: OK (chip=%d)\n", applied2);
    return 0;
}
