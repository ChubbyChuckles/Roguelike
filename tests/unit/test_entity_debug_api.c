#define SDL_MAIN_HANDLED 1
#include "../../src/core/app/app_state.h"
#include "../../src/core/entities/entity_debug.h"
#include "../../src/entities/player.h"
#include <assert.h>

/* Minimal smoke test for entity_debug APIs used by the Entities overlay panel.
   Validates: count/list/get_info/teleport/kill/spawn_at_player. */
int main(void)
{
#if defined(_MSC_VER)
    rogue_app_state_maybe_init();
#endif
    /* Ensure player exists (spawn APIs use player position as reference) */
    rogue_player_init(&g_app.player);
    g_app.player.base.pos.x = 0.0f;
    g_app.player.base.pos.y = 0.0f;
    /* Enable a basic enemy type so the spawn helper can succeed */
    g_app.enemy_type_count = 1;

    /* Snapshot initial enemy count (fresh test process should start at 0) */
    int initial = rogue_entity_debug_count();
    assert(initial >= 0);

    /* Spawn one hostile enemy near the player */
    int slot = rogue_entity_debug_spawn_at_player(1.5f, -0.5f);
    assert(slot >= 0);
    assert(rogue_entity_debug_count() == initial + 1);

    /* List should contain the spawned slot */
    int idxs[8] = {0};
    int n = rogue_entity_debug_list(idxs, 8);
    assert(n >= 1);
    int found = 0;
    for (int i = 0; i < n; ++i)
        if (idxs[i] == slot)
            found = 1;
    assert(found);

    /* Get info and validate basic invariants */
    RogueEntityDebugInfo info;
    assert(rogue_entity_debug_get_info(slot, &info) == 0);
    assert(info.alive == 1);
    assert(info.slot_index == slot);

    /* Teleport entity to an exact new position and verify */
    float nx = 12.25f, ny = -7.75f;
    assert(rogue_entity_debug_teleport(slot, nx, ny) == 0);
    RogueEntityDebugInfo info2;
    assert(rogue_entity_debug_get_info(slot, &info2) == 0);
    assert(info2.x == nx && info2.y == ny);

    /* Kill and verify removal from count and alive flag */
    assert(rogue_entity_debug_kill(slot) == 0);
    RogueEntityDebugInfo info3;
    assert(rogue_entity_debug_get_info(slot, &info3) == 0);
    assert(info3.alive == 0);
    assert(rogue_entity_debug_count() == initial);

    return 0;
}
