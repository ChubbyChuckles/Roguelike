#include "entities/enemy.h"
#include "entities/player.h"
#include "game/hit_system.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void setup_player(RoguePlayer* p, float x, float y, int facing)
{
    memset(p, 0, sizeof *p);
    p->base.pos.x = x;
    p->base.pos.y = y;
    p->facing = facing;
    p->equipped_weapon_id = 0;
}
static void setup_enemy(RogueEnemy* e, float x, float y)
{
    memset(e, 0, sizeof *e);
    e->base.pos.x = x;
    e->base.pos.y = y;
    e->alive = 1;
}

/* Opaque forward struct mimic */
struct RoguePlayerCombat
{
    int phase;
}; /* ensure size minimal; we only set phase */
#ifdef main
#undef main
#endif

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    rogue_weapon_hit_geo_ensure_default();
    RoguePlayer p;
    setup_player(&p, 10.0f, 10.0f, 2); /* right */
    RogueEnemy enemies[4];
    setup_enemy(&enemies[0], 11.2f, 10.0f);
    setup_enemy(&enemies[1], 12.5f, 10.0f);
    setup_enemy(&enemies[2], 10.0f, 11.0f);
    setup_enemy(&enemies[3], 14.0f, 14.0f); /* far */
    struct RoguePlayerCombat pc;
    pc.phase = 2; /* ROGUE_ATTACK_STRIKE enum value (ordering assumption) */
    rogue_hit_sweep_reset();
    int hc = rogue_combat_weapon_sweep_apply(&pc, &p, enemies, 4);
    assert(hc > 0);
    const RogueHitDebugFrame* dbg = rogue_hit_debug_last();
    assert(dbg && dbg->hit_count == hc);
    for (int i = 0; i < dbg->hit_count; i++)
    {
        float nx = dbg->normals[i][0];
        float ny = dbg->normals[i][1];
        float len = (float) sqrt(nx * nx + ny * ny);
        assert(len > 0.98f && len < 1.02f);
    }
    /* Reset and re-run; should still produce hits (mask cleared) */
    rogue_hit_sweep_reset();
    hc = rogue_combat_weapon_sweep_apply(&pc, &p, enemies, 4);
    assert(hc > 0);
    printf("hit_sweep_normals: PASS (hc=%d)\n", hc);
    return 0;
}
