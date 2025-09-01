#include "../../src/core/app/app_state.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

static void reset_world(void)
{
    rogue_effect_reset();
    memset(&g_app, 0, sizeof g_app);
    /* Place player at origin */
    g_app.player.base.pos.x = 0.0f;
    g_app.player.base.pos.y = 0.0f;
}

static void setup_two_enemies_in_radius(void)
{
    g_app.enemy_count = 2;
    memset(g_app.enemies, 0, sizeof g_app.enemies);
    g_app.enemies[0].alive = 1;
    g_app.enemies[0].health = 50;
    g_app.enemies[0].base.pos.x = 0.5f;
    g_app.enemies[0].base.pos.y = 0.0f;

    g_app.enemies[1].alive = 1;
    g_app.enemies[1].health = 60;
    g_app.enemies[1].base.pos.x = 1.0f;
    g_app.enemies[1].base.pos.y = 0.0f;
}

int main(void)
{
    /* DAMAGE: hits first alive enemy only */
    reset_world();
    g_app.enemy_count = 2;
    memset(g_app.enemies, 0, sizeof g_app.enemies);
    g_app.enemies[0].alive = 1;
    g_app.enemies[0].health = 40;
    g_app.enemies[1].alive = 1;
    g_app.enemies[1].health = 70;

    RogueEffectSpec dmg;
    memset(&dmg, 0, sizeof dmg);
    dmg.kind = ROGUE_EFFECT_DAMAGE;
    dmg.magnitude = 15;
    int id_dmg = rogue_effect_register(&dmg);
    assert(id_dmg >= 0);
    rogue_effect_apply(id_dmg, 0.0);
    /* First enemy takes at least 1 damage; mitigation can change exact amount but should reduce */
    assert(g_app.enemies[0].health < 40);
    assert(g_app.enemies[1].health == 70);

    /* AOE_BLAST: within radius affects both */
    reset_world();
    setup_two_enemies_in_radius();
    RogueEffectSpec aoe;
    memset(&aoe, 0, sizeof aoe);
    aoe.kind = ROGUE_EFFECT_AOE_BLAST;
    aoe.magnitude = 10;
    aoe.aura_radius = 1.5f; /* should catch both */
    int id_aoe = rogue_effect_register(&aoe);
    assert(id_aoe >= 0);
    rogue_effect_apply(id_aoe, 0.0);
    /* Both enemies should be damaged (health decreased) */
    assert(g_app.enemies[0].health < 50);
    assert(g_app.enemies[1].health < 60);

    return 0;
}
