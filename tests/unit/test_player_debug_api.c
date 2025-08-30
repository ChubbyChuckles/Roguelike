#include "../../src/core/app/app_state.h"
#include "../../src/core/player/player_debug.h"
#include "../../src/entities/player.h"
#include <assert.h>

int main(void)
{
    /* Ensure app state has defaults and player derived stats valid */
#if defined(_MSC_VER)
    rogue_app_state_maybe_init();
#endif
    rogue_player_init(&g_app.player);

    /* Health/mana/AP clamps */
    rogue_player_debug_set_health(999999);
    assert(rogue_player_debug_get_health() == rogue_player_debug_get_max_health());
    rogue_player_debug_set_mana(-123);
    assert(rogue_player_debug_get_mana() == 0);
    rogue_player_debug_set_ap(999999);
    assert(rogue_player_debug_get_ap() == rogue_player_debug_get_max_ap());

    /* Stats setter clamps to >=0 and recomputes derived (max_* increase when vitality rises) */
    int hpmax_before = rogue_player_debug_get_max_health();
    int vit_before = rogue_player_debug_get_stat(ROGUE_STAT_VITALITY);
    rogue_player_debug_set_stat(ROGUE_STAT_VITALITY, vit_before + 10);
    int hpmax_after = rogue_player_debug_get_max_health();
    assert(hpmax_after > hpmax_before);

    /* God mode bypass path: melee apply returns 0 damage when enabled. */
    extern int rogue_player_apply_incoming_melee(RoguePlayer*, float, float, float, int, int*,
                                                 int*);
    rogue_player_debug_set_god_mode(1);
    int blocked = -1, perfect = -1;
    int dmg =
        rogue_player_apply_incoming_melee(&g_app.player, 123.0f, 1.0f, 0.0f, 0, &blocked, &perfect);
    assert(dmg == 0);
    rogue_player_debug_set_god_mode(0);

    /* Noclip flag roundtrip */
    rogue_player_debug_set_noclip(1);
    assert(rogue_player_debug_get_noclip() == 1);
    rogue_player_debug_set_noclip(0);
    assert(rogue_player_debug_get_noclip() == 0);

    /* Teleport moves player position */
    rogue_player_debug_teleport(12.5f, -3.25f);
    assert(g_app.player.base.pos.x == 12.5f && g_app.player.base.pos.y == -3.25f);
    return 0;
}
