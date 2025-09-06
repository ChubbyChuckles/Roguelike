#include "../../src/core/app/app_state.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

/* Forwards from implementation (no public header yet) */
void rogue_effect_reset(void);
void rogue_spawn_entities_reset(void);
void rogue_spawn_entities_update(double now_ms);
int rogue_spawn_entity_active_count(void);
void rogue_effect_apply(int id, double now_ms);

static void reset_world(void)
{
    rogue_effect_reset();
    rogue_spawn_entities_reset();
    memset(&g_app, 0, sizeof g_app);
    g_app.tile_size = 32;
    g_app.player_frame_size = 32;
    g_app.player.base.pos.x = 4.0f;
    g_app.player.base.pos.y = 2.0f;
}

int main(void)
{
    reset_world();
    RogueEffectSpec s;
    memset(&s, 0, sizeof s);
    s.kind = ROGUE_EFFECT_SPAWN_ENTITY;
    s.spawn_entity_count = 3;
    s.spawn_entity_life_ms = 200.0f;
    int id = rogue_effect_register(&s);
    assert(id >= 0);
    assert(rogue_spawn_entity_active_count() == 0);
    rogue_effect_apply(id, 0.0);
    assert(rogue_spawn_entity_active_count() == 3);
    /* Advance beyond lifetime */
    rogue_spawn_entities_update(250.0);
    assert(rogue_spawn_entity_active_count() == 0);
    return 0;
}
