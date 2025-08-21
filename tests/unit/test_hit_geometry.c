#include "entities/enemy.h"
#include "entities/player.h"
#include "game/hit_system.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Handle possible SDL main macro remap */
#ifdef main
#undef main
#endif

/* Minimal stubs if not linked with full app */

static void setup_player(RoguePlayer* p)
{
    memset(p, 0, sizeof *p);
    p->base.pos.x = 5.0f;
    p->base.pos.y = 5.0f;
    p->facing = 2; /* right */
}
static void setup_enemy(RogueEnemy* e, float x, float y)
{
    memset(e, 0, sizeof *e);
    e->base.pos.x = x;
    e->base.pos.y = y;
    e->alive = 1;
}

int test_capsule_build_and_overlap()
{
    rogue_weapon_hit_geo_ensure_default();
    const RogueWeaponHitGeo* geo = rogue_weapon_hit_geo_get(0);
    assert(geo);
    RoguePlayer player;
    setup_player(&player);
    RogueCapsule cap;
    int ok = rogue_weapon_build_capsule(&player, geo, &cap);
    assert(ok);
    /* Facing right => x1 > x0, y same */
    assert(cap.x1 > cap.x0);
    RogueEnemy e;
    setup_enemy(&e, player.base.pos.x + geo->length * 0.9f, player.base.pos.y);
    int overlap = rogue_capsule_overlaps_enemy(&cap, &e);
    assert(overlap == 1);
    RogueEnemy far;
    setup_enemy(&far, player.base.pos.x + geo->length * 2.2f, player.base.pos.y);
    int overlap_far = rogue_capsule_overlaps_enemy(&cap, &far);
    assert(overlap_far == 0);
    return 0;
}

int test_facings()
{
    rogue_weapon_hit_geo_ensure_default();
    const RogueWeaponHitGeo* geo = rogue_weapon_hit_geo_get(0);
    assert(geo);
    for (int facing = 0; facing < 4; ++facing)
    {
        RoguePlayer p;
        setup_player(&p);
        p.facing = facing;
        RogueCapsule c;
        int ok = rogue_weapon_build_capsule(&p, geo, &c);
        assert(ok);
        if (facing == 0)
            assert(c.y1 > c.y0); /* down */
        if (facing == 1)
            assert(c.x1 < c.x0); /* left */
        if (facing == 2)
            assert(c.x1 > c.x0); /* right */
        if (facing == 3)
            assert(c.y1 < c.y0); /* up */
    }
    return 0;
}

int run_hit_geometry_tests()
{
    test_capsule_build_and_overlap();
    test_facings();
    printf("hit_geometry_tests: PASS\n");
    return 0;
}

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    return run_hit_geometry_tests();
}
