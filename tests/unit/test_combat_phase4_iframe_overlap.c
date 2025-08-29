#define SDL_MAIN_HANDLED
#include "../../src/entities/player.h"
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>

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

int main()
{
    RoguePlayer p;
    rogue_player_init(&p);
    g_exposed_player_for_stats = p;
    rogue_player_add_iframes(&p, 300.0f);
    /* Attempt to stack smaller grant (should keep 300) */
    rogue_player_add_iframes(&p, 100.0f);
    assert(p.iframes_ms == 300.0f);
    /* Extend with larger grant (should replace) */
    rogue_player_add_iframes(&p, 450.0f);
    assert(p.iframes_ms == 450.0f);
    /* Ensure incoming melee ignored while iframes active */
    int blocked = 0, perfect = 0;
    int dmg = rogue_player_apply_incoming_melee(&p, 50.0f, 0, 1, 10, &blocked, &perfect);
    assert(dmg == 0);
    /* Tick down partially then apply mid-way smaller grant (should not increase beyond remaining if
     * larger residual) */
    for (int i = 0; i < 10; i++)
    {
        rogue_player_update_reactions(&p, 16.0f);
    } /* 160ms elapsed */
    float remaining = p.iframes_ms;
    assert(remaining < 450.0f);
    rogue_player_add_iframes(&p,
                             remaining - 50.0f); /* smaller than current remaining -> unchanged */
    assert(p.iframes_ms == remaining);
    printf("phase4_iframe_overlap: OK\n");
    return 0;
}
