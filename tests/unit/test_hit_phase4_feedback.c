#include "../../src/game/hit_feedback.h"
#include <assert.h>
#include <stdio.h>

#ifdef main
#undef main
#endif

int main()
{
    /* Knockback magnitude scaling tests */
    float m_eq = rogue_hit_calc_knockback_mag(5, 5, 10, 10);
    float m_lvl = rogue_hit_calc_knockback_mag(10, 5, 10, 10);
    float m_str = rogue_hit_calc_knockback_mag(5, 5, 40, 5);
    assert(m_lvl > m_eq - 1e-4f);
    assert(m_str > m_eq - 1e-4f);
    float m_cap = rogue_hit_calc_knockback_mag(200, 1, 500, 1);
    assert(m_cap <= 0.55f + 1e-4f);
    /* Particle spawning range */
    int c_norm = rogue_hit_particles_spawn_impact(0, 0, 0, 1, 0);
    assert(c_norm >= 10 && c_norm <= 32);
    int c_over = rogue_hit_particles_spawn_impact(0, 0, 0, 1, 1);
    assert(c_over >= c_norm); /* explosion spawns more */
    printf("hit_phase4_feedback PASS (mag %.3f->%.3f cap %.3f particles %d/%d)\n", m_eq, m_lvl,
           m_cap, c_norm, c_over);
    return 0;
}
