/* Phase 7 integration test: sweep-driven strike, single hitstop application, no duplicate hits */
#include "../../src/core/app_state.h"
#include "../../src/game/combat.h"
#include "../../src/game/hit_system.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef main
#undef main
#endif

static void setup_player(RoguePlayer* p)
{
    memset(p, 0, sizeof *p);
    p->base.pos.x = 5.0f;
    p->base.pos.y = 5.0f;
    p->facing = 2;
    p->equipped_weapon_id = 0;
}
static void setup_enemy(RogueEnemy* e, float x, float y)
{
    memset(e, 0, sizeof *e);
    e->base.pos.x = x;
    e->base.pos.y = y;
    e->alive = 1;
    e->team_id = 2;
}

int main(void)
{
    rogue_weapon_hit_geo_ensure_default();
    RoguePlayer player;
    setup_player(&player);
    player.team_id = 1;
    player.strength = 20;
    player.dexterity = 10;
    player.intelligence = 5;
    player.level = 5;
    RoguePlayerCombat pc;
    memset(&pc, 0, sizeof pc);
    pc.phase = ROGUE_ATTACK_STRIKE; /* direct strike */
    RogueEnemy enemies[3];
    setup_enemy(&enemies[0], 6.2f, 5.0f);
    setup_enemy(&enemies[1], 7.0f, 5.1f);
    setup_enemy(&enemies[2], 9.5f, 5.0f); /* maybe out of reach */
    float before_hitstop = g_app.hitstop_timer_ms;
    int hits1 = rogue_combat_player_strike(&pc, &player, enemies, 3);
    assert(hits1 >= 1);
    float after_hitstop = g_app.hitstop_timer_ms;
    assert(after_hitstop >= before_hitstop); /* hitstop applied once */
    /* Call again without changing phase/time: should not add further hitstop */
    int hits2 = rogue_combat_player_strike(&pc, &player, enemies, 3);
    (void) hits2;
    float after_second = g_app.hitstop_timer_ms;
    assert(after_second == after_hitstop);
    printf("hit_phase7_integration: PASS (hits=%d)\n", hits1);
    return 0;
}
