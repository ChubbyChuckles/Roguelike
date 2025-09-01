#include "../../src/core/app/app_state.h"
#include "../../src/core/projectiles/projectiles.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

static void reset_world(void)
{
    rogue_effect_reset();
    rogue_projectiles_init();
    memset(&g_app, 0, sizeof g_app);
    g_app.tile_size = 32;
    g_app.player_frame_size = 32;
    g_app.player.base.pos.x = 10.0f;
    g_app.player.base.pos.y = 5.0f;
}

int main(void)
{
    reset_world();
    RogueEffectSpec s;
    memset(&s, 0, sizeof s);
    s.kind = ROGUE_EFFECT_SPAWN_PROJECTILE;
    s.magnitude = 7; /* damage */
    s.proj_speed = 8.0f;
    s.proj_life_ms = 1234.0f;
    s.proj_count = 1;
    int id = rogue_effect_register(&s);
    assert(id >= 0);
    assert(rogue_projectiles_active_count() == 0);
    /* Facing up (dy=1) by default */
    g_app.player.facing = 0;
    rogue_effect_apply(id, 0.0);
    assert(rogue_projectiles_active_count() == 1);
    assert(rogue_projectiles_last_damage() == 7);
    /* Advance time a bit to ensure projectile updates without crashing */
    for (int i = 0; i < 10; ++i)
        rogue_projectiles_update(16.0f);
    /* Now test count>1 spawns */
    reset_world();
    memset(&s, 0, sizeof s);
    s.kind = ROGUE_EFFECT_SPAWN_PROJECTILE;
    s.magnitude = 3;
    s.proj_speed = 5.0f;
    s.proj_life_ms = 500.0f;
    s.proj_count = 3;
    id = rogue_effect_register(&s);
    g_app.player.facing = 2; /* right */
    rogue_effect_apply(id, 0.0);
    assert(rogue_projectiles_active_count() == 3);
    assert(rogue_projectiles_last_damage() == 3);
    return 0;
}
