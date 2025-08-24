#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include "../../src/core/projectiles/projectiles.h"
#include <assert.h>
#include <stdio.h>

/* Minimal harness: build a tree with single ranged fire node gated by line-clear flag. */
int main(void)
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    /* Seed agent & target positions */
    rogue_bb_set_vec2(&bb, "agent_pos", 10.0f, 5.0f);
    rogue_bb_set_vec2(&bb, "target_pos", 12.0f, 5.0f);
    /* Gate flag false initially */
    rogue_bb_set_bool(&bb, "line_clear", false);
    rogue_bb_set_timer(&bb, "cool", 1.7f);

    /* Init projectile system to ensure deterministic clean state */
    rogue_projectiles_init();
    int before = rogue_projectiles_active_count();

    RogueBTNode* fire = rogue_bt_action_ranged_fire_projectile(
        "fire", "agent_pos", "target_pos", "line_clear", "cool", 6.0f, 1200.0f, 7);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(fire);

    /* Tick with flag false: should fail, no projectile */
    RogueBTStatus s0 = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(s0 == ROGUE_BT_FAILURE);
    assert(rogue_projectiles_active_count() == before);

    /* Enable flag and tick: should fire once */
    rogue_bb_set_bool(&bb, "line_clear", true);
    RogueBTStatus s1 = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(s1 == ROGUE_BT_SUCCESS);
    int after = rogue_projectiles_active_count();
    assert(after == before + 1);

    /* Cooldown should reset to ~0 */
    float cool = 1.0f;
    int ok = rogue_bb_get_timer(&bb, "cool", &cool);
    assert(ok && cool == 0.0f);

    rogue_behavior_tree_destroy(tree);
    (void) printf("test_ai_phase6_1_ranged_projectile OK\n");
    return 0;
}
