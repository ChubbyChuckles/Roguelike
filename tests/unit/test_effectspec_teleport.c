#include "../../src/core/app/app_state.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

static void init_world(void)
{
    memset(&g_app, 0, sizeof g_app);
    rogue_app_state_maybe_init();
    g_app.headless = 1;
    g_app.world_map.width = 100;
    g_app.world_map.height = 100;
    g_app.player.base.pos.x = 10.0f;
    g_app.player.base.pos.y = 10.0f;
}

int main(void)
{
    init_world();
    rogue_effect_reset();

    RogueEffectSpec tp = {0};
    tp.kind = ROGUE_EFFECT_TELEPORT;
    tp.magnitude = 7; /* tiles forward */

    int id = rogue_effect_register(&tp);
    assert(id >= 0);

    double t = 0.0;
    g_app.player.facing = 2; /* right */
    rogue_effect_apply(id, t);
    assert((int) g_app.player.base.pos.x == 17);
    assert((int) g_app.player.base.pos.y == 10);

    /* Test bounds clamp moving left beyond 0 */
    g_app.player.facing = 1; /* left */
    tp.magnitude = 50;
    int id2 = rogue_effect_register(&tp);
    rogue_effect_apply(id2, t);
    assert((int) g_app.player.base.pos.x == 0);

    puts("EFFECTSPEC_TELEPORT_OK");
    return 0;
}
