#define SDL_MAIN_HANDLED
#include "entities/player.h"
#include "game/combat.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

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

/* Guard cone edge tolerance: ensure dot slightly below threshold fails, at threshold succeeds,
 * slightly above succeeds. */
int main()
{
    RoguePlayer p;
    rogue_player_init(&p);
    g_exposed_player_for_stats = p;
    p.facing = 0; /* facing down (0,1) */
    int ok = rogue_player_begin_guard(&p, 0);
    assert(ok == 1);
    float fdx = 0.0f, fdy = 1.0f; /* facing vector */
    /* We craft attack dirs with given dot vs facing by rotating slightly. Facing (0,1); vector
     * (sin(theta), cos(theta)) gives dot=cos(theta). */
    float thresh = ROGUE_GUARD_CONE_DOT;  /* 0.25 */
    float theta_at = acosf(thresh);       /* angle that yields exactly threshold */
    float theta_below = theta_at + 0.01f; /* slightly more angle -> lower dot */
    float theta_above = theta_at - 0.01f; /* slightly less angle -> higher dot */
    int blocked = 0, perfect = 0;
    int dmg = 100;
    /* Below threshold: expect full damage (not blocked) */
    blocked = 0;
    perfect = 0;
    int applied_below = rogue_player_apply_incoming_melee(
        &p, (float) dmg, sinf(theta_below), cosf(theta_below), 15, &blocked, &perfect);
    assert(blocked == 0 && applied_below == dmg);
    /* Reset guard active time for perfect guard neutrality (not checking perfect vs normal here) */
    p.guard_active_time_ms = 10.0f;
    /* At threshold: treat as blocked */
    blocked = 0;
    perfect = 0;
    int applied_at = rogue_player_apply_incoming_melee(&p, (float) dmg, sinf(theta_at),
                                                       cosf(theta_at), 15, &blocked, &perfect);
    assert(blocked == 1);
    /* Above threshold: also blocked */
    p.guard_active_time_ms = 10.0f;
    blocked = 0;
    perfect = 0;
    int applied_above = rogue_player_apply_incoming_melee(
        &p, (float) dmg, sinf(theta_above), cosf(theta_above), 15, &blocked, &perfect);
    assert(blocked == 1);
    printf("phase3_guard_cone_edge: OK (below=%d at=%d above=%d)\n", applied_below, applied_at,
           applied_above);
    return 0;
}
