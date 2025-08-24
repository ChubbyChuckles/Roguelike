#define SDL_MAIN_HANDLED 1
#include "../../src/ai/core/ai_debug.h"
#include "../../src/ai/core/ai_trace.h"
#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Simple factory producing a 2-level tree */
static RogueBTStatus noop_tick_fn(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) n;
    (void) bb;
    (void) dt;
    return ROGUE_BT_RUNNING;
}
static RogueBTStatus child_tick_fn(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) n;
    (void) bb;
    (void) dt;
    return ROGUE_BT_SUCCESS;
}
static RogueBehaviorTree* factory(void)
{
    RogueBTNode* root = rogue_bt_node_create("Root", 2, noop_tick_fn);
    RogueBTNode* a = rogue_bt_node_create("ChildA", 0, child_tick_fn);
    RogueBTNode* b = rogue_bt_node_create("ChildB", 0, child_tick_fn);
    rogue_bt_node_add_child(root, a);
    rogue_bt_node_add_child(root, b);
    return rogue_behavior_tree_create(root);
}

int main(void)
{
    RogueBehaviorTree* t = factory();
    printf("PH10 start\n");
    char buf[512];
    int n = rogue_ai_bt_visualize(t, buf, sizeof buf);
    printf("viz:%d\n", n);
    if (n <= 0 || !strstr(buf, "Root") || !strstr(buf, "ChildA"))
    {
        printf("AI_DBG_FAIL viz\n");
        return 1;
    }
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    rogue_bb_set_int(&bb, "hp", 42);
    rogue_bb_set_vec2(&bb, "pos", 1, 2);
    char bbout[256];
    int dn = rogue_ai_blackboard_dump(&bb, bbout, sizeof bbout);
    printf("dump:%d\n", dn);
    if (dn <= 0 || !strstr(bbout, "hp=42") || !strstr(bbout, "pos=(1.00,2.00)"))
    {
        printf("AI_DBG_FAIL bb_dump\n");
        return 1;
    }
    // Trace export
    RogueAITraceBuffer tb;
    rogue_ai_trace_init(&tb);
    rogue_ai_trace_push(&tb, 1, 123);
    rogue_ai_trace_push(&tb, 2, 456);
    char json[256];
    int jn = rogue_ai_trace_export_json(&tb, json, sizeof json);
    printf("json:%d %s\n", jn, json);
    if (jn <= 0 || !strstr(json, "123") || !strstr(json, "456"))
    {
        printf("AI_DBG_FAIL trace_json\n");
        return 1;
    }
    // Determinism verify
    uint64_t hash = 0;
    int det = rogue_ai_determinism_verify(factory, 5, &hash);
    printf("det:%d hash:%llu\n", det, (unsigned long long) hash);
    if (!det || hash == 0)
    {
        printf("AI_DBG_FAIL determinism\n");
        return 1;
    }
    printf("AI_DBG_OK viz=%d dump=%d json=%d hash=%llu\n", n, dn, jn, (unsigned long long) hash);
    rogue_behavior_tree_destroy(t);
    return 0;
}
