/* Phase 16.3/16.4 need/greed + trade validation tests */
#include "../../src/core/app_state.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_multiplayer.h"
#include <stdio.h>

static int fail = 0;
#define CHECK(c, msg)                                                                              \
    do                                                                                             \
    {                                                                                              \
        if (!(c))                                                                                  \
        {                                                                                          \
            printf("FAIL:%s line %d %s\n", __FILE__, __LINE__, msg);                               \
            fail = 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    rogue_items_init_runtime();
    /* spawn item and start need/greed among 3 players (0,1,2) */
    int inst = rogue_items_spawn(0, 1, 0, 0);
    CHECK(inst >= 0, "spawn");
    int players[3] = {0, 1, 2};
    CHECK(rogue_loot_need_greed_begin(inst, players, 3) == 0, "begin");
    /* player0 greed, player1 need, player2 pass */
    int r0 = rogue_loot_need_greed_choose(inst, 0, 0, 0);
    CHECK(r0 >= 400 && r0 < 700, "p0_greed_range");
    int r1 = rogue_loot_need_greed_choose(inst, 1, 1, 0);
    CHECK(r1 >= 700 && r1 < 1000, "p1_need_range");
    CHECK(rogue_loot_need_greed_choose(inst, 2, 0, 1) == -3, "p2_pass_code");
    int winner = rogue_loot_need_greed_resolve(inst);
    CHECK(winner == 1, "winner_is_p1_need");
    CHECK(!rogue_loot_instance_locked(inst), "unlocked_after_resolve");
    CHECK(g_app.item_instances[inst].owner_player_id == 1, "ownership_assigned");
    /* trade from 1 to 0 */
    CHECK(rogue_loot_trade_request(inst, 1, 0) == 0, "trade_ok");
    CHECK(g_app.item_instances[inst].owner_player_id == 0, "ownership_traded");
    /* cannot trade if not owner */
    CHECK(rogue_loot_trade_request(inst, 2, 1) < 0, "trade_reject_not_owner");
    if (fail)
    {
        printf("FAILURES\n");
        return 1;
    }
    printf("OK:loot_phase16_need_greed_trade\n");
    return 0;
}
