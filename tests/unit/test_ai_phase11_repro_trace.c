#include "ai/core/ai_rng.h"
#include "ai/core/ai_trace.h"
#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* RNG branch leaf: returns SUCCESS if rng next value even else FAILURE */
static RogueBTStatus rng_branch_tick(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) dt;
    RogueAIRNG* rng = (RogueAIRNG*) n->user_data;
    uint32_t v = rogue_ai_rng_next_u32(rng);
    return (v & 1u) ? ROGUE_BT_FAILURE : ROGUE_BT_SUCCESS;
}

static RogueBehaviorTree* build_tree(RogueAIRNG* rng)
{
    RogueBTNode* root = rogue_bt_selector("sel");
    RogueBTNode* rngleaf = rogue_bt_node_create("rng", 0, rng_branch_tick);
    rngleaf->user_data = rng;
    RogueBTNode* fallback = rogue_bt_leaf_always_success("fallback");
    rogue_bt_node_add_child(root, rngleaf);
    rogue_bt_node_add_child(root, fallback);
    return rogue_behavior_tree_create(root);
}

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

int main(void)
{
    RogueAIRNG r1, r2;
    rogue_ai_rng_seed(&r1, 123);
    rogue_ai_rng_seed(&r2, 123);
    RogueBehaviorTree* t1 = build_tree(&r1);
    RogueBehaviorTree* t2 = build_tree(&r2);
    char path1[128], path2[128];
    uint32_t hashes1[64], hashes2[64];
    int count = 40;
    assert(count <= 64);
    for (int i = 0; i < count; i++)
    {
        rogue_behavior_tree_tick(t1, NULL, 0.016f);
        rogue_behavior_tree_tick(t2, NULL, 0.016f);
        int n1 = rogue_behavior_tree_serialize_active_path(t1, path1, sizeof path1);
        int n2 = rogue_behavior_tree_serialize_active_path(t2, path2, sizeof path2);
        assert(n1 == n2 && strcmp(path1, path2) == 0);
        hashes1[i] = fnv1a32(path1, (size_t) n1);
        hashes2[i] = fnv1a32(path2, (size_t) n2);
        assert(hashes1[i] == hashes2[i]);
    }
    rogue_behavior_tree_destroy(t1);
    rogue_behavior_tree_destroy(t2);
    /* Divergent seeds produce different sequence somewhere */
    rogue_ai_rng_seed(&r1, 123);
    rogue_ai_rng_seed(&r2, 124);
    t1 = build_tree(&r1);
    t2 = build_tree(&r2);
    int diverged = 0;
    for (int i = 0; i < count; i++)
    {
        rogue_behavior_tree_tick(t1, NULL, 0.016f);
        rogue_behavior_tree_tick(t2, NULL, 0.016f);
        int n1 = rogue_behavior_tree_serialize_active_path(t1, path1, sizeof path1);
        int n2 = rogue_behavior_tree_serialize_active_path(t2, path2, sizeof path2);
        if (n1 != n2 || strcmp(path1, path2) != 0)
        {
            diverged = 1;
            break;
        }
    }
    rogue_behavior_tree_destroy(t1);
    rogue_behavior_tree_destroy(t2);
    assert(diverged == 1);
    printf("AI_PHASE11_REPRO_TRACE_OK matched+diverged sequences\n");
    return 0;
}
