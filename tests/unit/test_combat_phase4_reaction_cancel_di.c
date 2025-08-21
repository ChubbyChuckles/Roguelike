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

static void reset(RoguePlayer* p)
{
    rogue_player_init(p);
    p->poise = p->poise_max;
    p->reaction_type = 0;
    p->reaction_timer_ms = 0;
}

static void advance(RoguePlayer* p, float ms)
{
    while (ms > 0)
    {
        float step = ms > 16 ? 16 : ms;
        rogue_player_update_reactions(p, step);
        ms -= step;
    }
}

int main()
{
    RoguePlayer p;
    reset(&p);
    g_exposed_player_for_stats = p;
    /* Trigger stagger (type=2) via poise break */
    p.poise = 5.0f;
    int blocked = 0, perfect = 0;
    int dmg = rogue_player_apply_incoming_melee(&p, 20.0f, 0, 1, 10, &blocked, &perfect);
    (void) dmg;
    assert(p.reaction_type == 2);
    float total = p.reaction_total_ms;
    assert(total > 0);
    /* Attempt early cancel too soon (before 55% for stagger) */
    advance(&p, total * 0.50f);
    int canceled = rogue_player_try_reaction_cancel(&p);
    assert(!canceled && p.reaction_type == 2);
    /* Advance into window (at 60%) and cancel */
    float remaining = p.reaction_timer_ms;
    float elapsed = p.reaction_total_ms - remaining;
    float target_frac = 0.60f;
    float needed = target_frac * p.reaction_total_ms - elapsed;
    advance(&p, needed + 1.0f);
    canceled = rogue_player_try_reaction_cancel(&p);
    assert(canceled && p.reaction_type == 0);
    /* Apply knockdown and test DI accumulation & cap */
    rogue_player_apply_reaction(&p, 3);
    assert(p.reaction_type == 3);
    for (int i = 0; i < 200; i++)
    {
        rogue_player_apply_reaction_di(&p, 1, 0);
    } /* push x positive */
    double mag = sqrt(p.reaction_di_accum_x * p.reaction_di_accum_x +
                      p.reaction_di_accum_y * p.reaction_di_accum_y);
    assert(mag <= p.reaction_di_max + 1e-4);
    /* Cannot cancel after already canceled early (flag resets with new reaction) */
    advance(&p, p.reaction_total_ms * 0.70f);
    canceled = rogue_player_try_reaction_cancel(&p);
    assert(canceled && p.reaction_type == 0);
    printf("phase4_reaction_cancel_di: OK\n");
    return 0;
}
