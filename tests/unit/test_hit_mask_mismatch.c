/* Slice C mismatch logging test */
#include "entities/enemy.h"
#include "entities/player.h"
#include "game/combat.h"
#include "game/hit_pixel_mask.h"
#include "game/hit_system.h"
#include <stdio.h>
#include <string.h>

#ifdef main
#undef main
#endif

/* We craft a scenario where capsule hits an enemy outside placeholder mask range so we get
 * capsule-only mismatch. */
int main(void)
{
    rogue_hit_mismatch_counters_reset();
    g_hit_use_pixel_masks = 1; /* enable pixel path */
    RoguePlayer p;
    memset(&p, 0, sizeof p);
    p.base.pos.x = 0;
    p.base.pos.y = 0;
    p.anim_frame = 0;
    p.equipped_weapon_id = 0;
    p.facing = 2; /* right */
    RoguePlayerCombat pc;
    memset(&pc, 0, sizeof pc);
    pc.phase = ROGUE_ATTACK_STRIKE;
    RogueEnemy enemies[3];
    memset(enemies, 0, sizeof enemies);
    for (int i = 0; i < 3; i++)
    {
        enemies[i].alive = 1;
    }
    /* Placeholder mask line spans x range roughly 0..24 advancing; frame0 advance=0 so mask covers
     * x=0..24 at y 6..9. */
    enemies[0].base.pos.x = 10.0f;
    enemies[0].base.pos.y = 7.0f; /* inside mask -> both */
    enemies[1].base.pos.x = 30.0f;
    enemies[1].base.pos.y = 7.0f; /* outside mask bar -> neither path -> ignored */
    enemies[2].base.pos.x = 1.2f;
    enemies[2].base.pos.y = 0.0f; /* near player within capsule reach but y not inside mask stripe
                                     (mask y ~6..9) so capsule-only mismatch */
    int hc = rogue_combat_weapon_sweep_apply(&pc, &p, enemies, 3);
    const RogueHitDebugFrame* dbg = rogue_hit_debug_last();
    int total_pix_only = 0, total_cap_only = 0;
    rogue_hit_mismatch_counters(&total_pix_only, &total_cap_only);
    /* Expect at least one capsule-only mismatch (enemy index 2) */
    if (total_cap_only < 1)
    {
        printf("FAIL: expected capsule-only mismatch logged (got %d)\n", total_cap_only);
        return 1;
    }
    if (dbg == NULL)
    {
        printf("FAIL: no debug frame\n");
        return 1;
    }
    printf("PASS: mismatch logging cap_only=%d pix_only=%d auth_used=%c hits=%d\n", total_cap_only,
           total_pix_only, dbg->pixel_used ? 'P' : 'C', hc);
    return 0;
}
