#define SDL_MAIN_HANDLED 1
#include "../../src/core/app/app_state.h"
#include "../../src/core/progression/progression_mastery.h"
#include "../../src/core/progression/progression_specialization.h"
#include "../../src/core/skills/skills.h"
#include "../../src/game/buffs.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Minimal init for buffs & app time */
    g_app.game_time_ms = 1000.0;
    rogue_buffs_init();
    /* apply a buff and verify hash reflects it */
    int ok = rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE, 5, 5000.0, g_app.game_time_ms,
                               ROGUE_BUFF_STACK_REFRESH, 0);
    assert(ok == 1);
    uint64_t h1 = skill_export_active_buffs_hash(g_app.game_time_ms);
    assert(h1 != 0ull);
    /* advance time slightly without expiry; hash should change as remaining_ms changes */
    uint64_t h2 = skill_export_active_buffs_hash(g_app.game_time_ms + 10.0);
    assert(h1 != h2);

    /* Coefficient default without mastery/spec chosen should be ~1.0 */
    float c = skill_get_effective_coefficient(0);
    assert(c > 0.0f);
    /* Not asserting exact 1.0 to avoid future tuning changes; just basic sanity. */

    printf("OK\n");
    return 0;
}
