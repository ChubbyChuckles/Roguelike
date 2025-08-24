#include "../../src/ai/core/ai_rng.h"
#include "../../src/ai/core/ai_trace.h"
#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/basic_nodes.h"
#include "../../src/util/determinism.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t fnv1a32(const void* data, size_t len)
{
    const unsigned char* p = (const unsigned char*) data;
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < len; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

int main()
{
    // Build a simple tree: Selector( Sequence( cond_visible ), success_leaf )
    RogueBTNode* root = rogue_bt_selector("root");
    RogueBTNode* seq = rogue_bt_sequence("seq");
    RogueBTNode* cond = rogue_bt_leaf_check_bool("visible", "vis", true);
    RogueBTNode* fallback = rogue_bt_leaf_always_success("idle");
    rogue_bt_node_add_child(root, seq);
    rogue_bt_node_add_child(root, fallback);
    rogue_bt_node_add_child(seq, cond);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(root);

    RogueBlackboard bb;
    rogue_bb_init(&bb);
    RogueAITraceBuffer trace;
    rogue_ai_trace_init(&trace);

    char path[256];
    for (int i = 0; i < 5; i++)
    {
        RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
        assert(st == ROGUE_BT_SUCCESS);
        int n = rogue_behavior_tree_serialize_active_path(tree, path, (int) sizeof(path));
        assert(n > 0);
        uint32_t ph = fnv1a32(path, (size_t) n);
        rogue_ai_trace_push(&trace, tree->tick_count, ph);
    }
    assert(trace.count == 5);
    // Now set visibility so branch changes path content
    assert(rogue_bb_set_bool(&bb, "vis", true));
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS);
    int n = rogue_behavior_tree_serialize_active_path(tree, path, (int) sizeof(path));
    uint32_t ph = fnv1a32(path, (size_t) n);
    rogue_ai_trace_push(&trace, tree->tick_count, ph);
    assert(trace.count == 6);

    // Basic RNG determinism
    RogueAIRNG r1, r2;
    rogue_ai_rng_seed(&r1, 1234);
    rogue_ai_rng_seed(&r2, 1234);
    for (int i = 0; i < 16; i++)
    {
        assert(rogue_ai_rng_next_u32(&r1) == rogue_ai_rng_next_u32(&r2));
    }

    rogue_behavior_tree_destroy(tree);
    printf("[test_ai_phase1_scheduler_trace] Passed.\n");
    return 0;
}
