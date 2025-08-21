#define SDL_MAIN_HANDLED
#include "game/hitbox.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int feq(float a, float b) { return fabsf(a - b) < 1e-4f; }

int main()
{
    /* Capsule: segment (0,0)->(2,0) radius 0.5 */
    RogueHitbox cap = rogue_hitbox_make_capsule(0, 0, 2, 0, 0.5f);
    assert(rogue_hitbox_point_overlap(&cap, 1.0f, 0.0f));  /* along center */
    assert(rogue_hitbox_point_overlap(&cap, 0.0f, 0.0f));  /* endpoint */
    assert(!rogue_hitbox_point_overlap(&cap, 1.0f, 0.6f)); /* outside radius */

    /* Arc: center (0,0) radius 2, 0 to 90 deg (quadrant I) */
    const float PI_F = 3.14159265358979323846f;
    RogueHitbox arc = rogue_hitbox_make_arc(0, 0, 2.0f, 0.0f, PI_F * 0.5f, 0.0f);
    assert(rogue_hitbox_point_overlap(&arc, 1.0f, 1.0f));   /* inside */
    assert(!rogue_hitbox_point_overlap(&arc, -1.0f, 1.0f)); /* wrong quadrant */
    /* Inner radius exclusion */
    RogueHitbox ring_arc = rogue_hitbox_make_arc(0, 0, 2.0f, 0.0f, PI_F * 0.5f, 1.0f);
    assert(!rogue_hitbox_point_overlap(&ring_arc, 0.5f, 0.5f)); /* inside hole */
    assert(rogue_hitbox_point_overlap(&ring_arc, 1.2f, 0.4f));  /* in band */

    /* Chain: three points making an L shape */
    RogueHitbox chain = rogue_hitbox_make_chain(0.4f);
    rogue_hitbox_chain_add_point(&chain, 0, 0);
    rogue_hitbox_chain_add_point(&chain, 1, 0);
    rogue_hitbox_chain_add_point(&chain, 1, 1);
    assert(rogue_hitbox_point_overlap(&chain, 0.5f, 0.0f));  /* along first segment */
    assert(rogue_hitbox_point_overlap(&chain, 1.0f, 0.5f));  /* along second segment */
    assert(!rogue_hitbox_point_overlap(&chain, 0.8f, 0.8f)); /* off diagonal away from radius */

    /* Projectile spawn descriptor angles */
    RogueHitbox spawn = rogue_hitbox_make_projectile_spawn(5, 0, 0, 6.0f, PI_F, 0.0f);
    float expected_first = -PI_F * 0.5f;
    float expected_last = PI_F * 0.5f;
    float a0 = rogue_hitbox_projectile_spawn_angle(&spawn.u.proj, 0);
    float a4 = rogue_hitbox_projectile_spawn_angle(&spawn.u.proj, 4);
    assert(feq(a0, expected_first));
    assert(feq(a4, expected_last));
    float a2 = rogue_hitbox_projectile_spawn_angle(&spawn.u.proj, 2); /* middle -> center */
    assert(feq(a2, 0.0f));

    printf("phase5_hitbox_primitives: OK\n");
    return 0;
}
