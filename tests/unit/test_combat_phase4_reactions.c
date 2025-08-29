#define SDL_MAIN_HANDLED
#include "../../src/entities/player.h"
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

RoguePlayer g_exposed_player_for_stats = {0};
static int rogue_get_current_attack_frame(void) { return 3; }
static void rogue_app_add_hitstop(float ms) { (void) ms; }
static void rogue_add_damage_number(float x, float y, int amount, int from_player)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
}
static void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
    (void) crit;
}

static void reset_player(RoguePlayer* p)
{
    rogue_player_init(p);
    p->facing = 0;
    p->poise = p->poise_max;
    p->reaction_type = 0;
    p->reaction_timer_ms = 0;
}

int main()
{
    RoguePlayer p;
    reset_player(&p);
    g_exposed_player_for_stats = p;
    /* Light flinch: damage 30 triggers reaction_type=1 */
    int blocked = 0, perfect = 0;
    int dmg = rogue_player_apply_incoming_melee(&p, 30.0f, 0.0f, 1.0f, 5, &blocked, &perfect);
    assert(dmg == 30);
    assert(p.reaction_type == 1 && p.reaction_timer_ms > 0);
    /* Advance reaction update until it clears */
    float total = 0;
    while (p.reaction_type != 0 && total < 500)
    {
        rogue_player_update_reactions(&p, 16.0f);
        total += 16.0f;
    }
    assert(p.reaction_type == 0);
    /* Stagger from poise break: apply large poise damage steps */
    p.poise = 10.0f;
    p.reaction_type = 0;
    p.reaction_timer_ms = 0;
    dmg = rogue_player_apply_incoming_melee(&p, 10.0f, 0.0f, 1.0f, 15, &blocked, &perfect);
    assert(p.reaction_type == 2); /* stagger */
    /* Knockdown via high raw damage threshold >=80 */
    p.reaction_type = 0;
    p.reaction_timer_ms = 0;
    p.poise = p.poise_max; /* restore */
    dmg = rogue_player_apply_incoming_melee(&p, 100.0f, 0.0f, 1.0f, 0, &blocked, &perfect);
    assert(p.reaction_type == 3);
    /* I-frame immunity: grant iframes and ensure next hit ignored */
    p.iframes_ms = 200.0f;
    p.reaction_type = 0;
    p.reaction_timer_ms = 0;
    dmg = rogue_player_apply_incoming_melee(&p, 50.0f, 0.0f, 1.0f, 20, &blocked, &perfect);
    assert(dmg == 0); /* ignored */
    printf("phase4_reactions: OK\n");
    return 0;
}
