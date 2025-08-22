/**
 * @file ai_debug.c
 * @brief Debugging helpers for AI: visualization, blackboard dump, perception
 * primitives, trace export, and deterministic verification helpers.
 *
 * This module contains small utility functions used by tests and debug UIs
 * to inspect behavior trees, blackboards, perception agents, and AI traces.
 * The functions are intentionally simple and return text representations or
 * lightweight primitive lists for visualization overlays. Determinism
 * verification runs two trees in lockstep and compares their active paths.
 */
#include "ai/core/ai_debug.h"
#include "ai/core/ai_trace.h"
#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include "ai/perception/perception.h"
#include "util/determinism.h"
#include <stdio.h>
#include <string.h>

/* 10.1 Behavior tree visualization */
/**
 * @brief Recursive helper to serialize the tree node hierarchy into text.
 *
 * Walks the tree in pre-order and appends indented lines to the provided
 * buffer. Existing inline comment: Behavior tree visualization slice 10.1.
 *
 * @param node Node to visualize; NULL is a no-op.
 * @param out Destination buffer to append to; must be writable.
 * @param cap Capacity of the destination buffer in bytes.
 * @param depth Current depth used for indentation (callers supply 0).
 * @param written Pointer to current write offset into out; updated by this call.
 * @note This helper is internal (static) and does not validate all inputs
 *       beyond simple early-exit checks.
 */
static void viz_rec(RogueBTNode* node, char* out, int cap, int depth, int* written)
{
    if (!node || *written >= cap)
        return;
    char line[128];
    int len = snprintf(line, sizeof line, "%*s- %s\n", depth * 2, "",
                       node->debug_name ? node->debug_name : "?");
    if (*written + len < cap)
    {
        memcpy(out + *written, line, len);
        *written += len;
    }
    for (uint16_t i = 0; i < node->child_count; i++)
        viz_rec(node->children[i], out, cap, depth + 1, written);
}
/**
 * @brief Produce a human-readable visualization of a behavior tree.
 *
 * Traverses the tree rooted at tree->root and writes an indented list of
 * node names into the provided buffer. The output is NUL-terminated on
 * success. This function preserves the original behavior: it returns -1 on
 * invalid input and otherwise returns the number of bytes written (excluding
 * the trailing NUL), clamped to cap-1.
 *
 * @param tree Pointer to the behavior tree to visualize.
 * @param out Output buffer to receive the visualization text.
 * @param cap Size of the output buffer in bytes; must be > 0.
 * @return int Number of bytes written (excluding NUL) or -1 on invalid args.
 */
int rogue_ai_bt_visualize(struct RogueBehaviorTree* tree, char* out, int cap)
{
    if (!tree || !out || cap <= 0)
        return -1;
    int w = 0;
    viz_rec(tree->root, out, cap, 0, &w);
    if (w >= cap)
        w = cap - 1;
    out[w] = '\0';
    return w;
}

/* 10.3 Blackboard inspector */
/**
 * @brief Dump the contents of a blackboard into a human-readable string.
 *
 * Iterates over entries in the provided blackboard and appends lines of the
 * form key=value to the output buffer. Supported types (int/float/bool/ptr/vec2/timer)
 * are formatted in a compact, human-friendly form. Existing inline comment:
 * Blackboard inspector slice 10.3.
 *
 * @param bb Pointer to the blackboard to dump.
 * @param out Output buffer to receive the dump.
 * @param cap Capacity of the output buffer in bytes; must be > 0.
 * @return int Number of bytes written (excluding NUL) or -1 on invalid args.
 */
int rogue_ai_blackboard_dump(struct RogueBlackboard* bb, char* out, int cap)
{
    if (!bb || !out || cap <= 0)
        return -1;
    int w = 0;
    for (int i = 0; i < bb->count; i++)
    {
        RogueBBEntry* e = &bb->entries[i];
        char line[160];
        switch (e->type)
        {
        case ROGUE_BB_INT:
            snprintf(line, sizeof line, "%s=%d\n", e->key, e->v.i);
            break;
        case ROGUE_BB_FLOAT:
            snprintf(line, sizeof line, "%s=%.3f\n", e->key, e->v.f);
            break;
        case ROGUE_BB_BOOL:
            snprintf(line, sizeof line, "%s=%s\n", e->key, e->v.b ? "true" : "false");
            break;
        case ROGUE_BB_PTR:
            snprintf(line, sizeof line, "%s=%p\n", e->key, e->v.p);
            break;
        case ROGUE_BB_VEC2:
            snprintf(line, sizeof line, "%s=(%.2f,%.2f)\n", e->key, e->v.v2.x, e->v.v2.y);
            break;
        case ROGUE_BB_TIMER:
            snprintf(line, sizeof line, "%s=timer(%.2f)\n", e->key, e->v.timer);
            break;
        default:
            snprintf(line, sizeof line, "%s=?\n", e->key);
        }
        int len = (int) strlen(line);
        if (w + len < cap)
        {
            memcpy(out + w, line, len);
            w += len;
        }
        else
            break;
    }
    if (w >= cap)
        w = cap - 1;
    out[w] = '\0';
    return w;
}

/* 10.2 Perception overlay primitives */
/**
 * @brief Produce a small set of debug primitives representing perception state.
 *
 * Currently simplified to emit a facing line (from agent position along
 * facing vector scaled by vision_dist) and a line-of-sight ray to the player.
 * The function intentionally ignores fov_deg and is designed for lightweight
 * overlay rendering in debug views.
 *
 * @param a Perception agent to sample (position & facing).
 * @param player_x X coordinate of the player (for LOS ray endpoint).
 * @param player_y Y coordinate of the player.
 * @param out Preallocated array to receive primitives.
 * @param max Maximum number of primitives the out array can hold.
 * @param fov_deg Field-of-view in degrees (currently unused / reserved).
 * @param vision_dist Distance to project the facing vector for the cone line.
 * @return int Number of primitives written into out (0..max).
 */
int rogue_ai_perception_collect_debug(const RoguePerceptionAgent* a, float player_x, float player_y,
                                      RogueAIDebugPrimitive* out, int max, float fov_deg,
                                      float vision_dist)
{
    if (!a || !out || max <= 0)
        return 0;
    (void) fov_deg;
    (void) player_x;
    (void) player_y;
    int count = 0; // simplified; emit facing line only
    if (count < max)
    {
        out[count].ax = a->x;
        out[count].ay = a->y;
        out[count].bx = a->x + a->facing_x * vision_dist;
        out[count].by = a->y + a->facing_y * vision_dist;
        out[count].kind = ROGUE_AI_DEBUG_PRIM_FOV_CONE;
        count++;
    }
    // LOS ray to player
    if (count < max)
    {
        out[count].ax = a->x;
        out[count].ay = a->y;
        out[count].bx = player_x;
        out[count].by = player_y;
        out[count].kind = ROGUE_AI_DEBUG_PRIM_LOS_RAY;
        count++;
    }
    return count;
}

/* 10.4 Trace export JSON */
/**
 * @brief Serialize an AI trace buffer into a compact JSON array.
 *
 * Each trace entry is serialized as an object with tick and hash fields.
 * The function writes into the provided buffer up to cap bytes, NUL-terminating
 * the result. On invalid input returns -1; otherwise returns the number of
 * bytes written (excluding the trailing NUL) or the clamped value cap-1.
 *
 * @param tb Trace buffer to export.
 * @param out Output buffer for JSON text.
 * @param cap Capacity of the output buffer in bytes; must be > 0.
 * @return int Number of bytes written (excluding NUL) or -1 on invalid args.
 */
int rogue_ai_trace_export_json(const struct RogueAITraceBuffer* tb, char* out, int cap)
{
    if (!tb || !out || cap <= 0)
        return -1;
    int w = 0;
    if (w < cap)
        out[w++] = '[';
    for (int i = 0; i < tb->count; i++)
    {
        int idx = ((int) tb->cursor - tb->count + i);
        if (idx < 0)
            idx += ROGUE_AI_TRACE_CAP;
        const RogueAITraceEntry* e = &tb->entries[idx];
        char buf[64];
        int len = snprintf(buf, sizeof buf, "%s{\"tick\":%u,\"hash\":%u}", i ? "," : "",
                           e->tick_index, e->hash);
        if (w + len < cap)
        {
            memcpy(out + w, buf, len);
            w += len;
        }
        else
            break;
    }
    if (w < cap)
        out[w++] = ']';
    if (w >= cap)
        w = cap - 1;
    out[w] = '\0';
    return w;
}

/* 10.5 Determinism verifier */
/**
 * @brief Compute a simple FNV-1a hash of the serialized active path.
 *
 * Serializes the active path into a local buffer via
 * rogue_behavior_tree_serialize_active_path and computes FNV-1a32 over the
 * resulting bytes. Returns 0 if serialization fails.
 *
 * @param t Behavior tree to sample.
 * @return uint32_t FNV-1a32 of the serialized active path, or 0 on failure.
 */
static uint32_t path_hash(RogueBehaviorTree* t)
{
    char buf[256];
    int n = rogue_behavior_tree_serialize_active_path(t, buf, sizeof buf);
    if (n < 0)
        return 0; // simple FNV1a32
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++)
    {
        h ^= (unsigned char) buf[i];
        h *= 16777619u;
    }
    return h;
}
/**
 * @brief Run two independently created behavior trees for ticks steps and
 * verify they take identical active paths for every tick.
 *
 * This helper constructs two trees using factory(), ticks them in lockstep
 * with a fixed delta (0.016f), and compares the per-tick active path hash.
 * It accumulates a 64-bit FNV-1a over per-tick path hashes and returns it
 * via out_hash on success. Returns 1 on deterministic success, 0 on mismatch
 * or invalid inputs. Existing inline: Determinism verifier slice 10.5.
 *
 * @param factory Factory function that returns a newly allocated RogueBehaviorTree*.
 * @param ticks Number of tick iterations to run (>0).
 * @param out_hash Optional out parameter to receive the accumulated hash; may be NULL.
 * @return int 1 if deterministic across all ticks, 0 otherwise.
 */
int rogue_ai_determinism_verify(RogueAIBTFactory factory, int ticks, uint64_t* out_hash)
{
    if (!factory || ticks <= 0)
        return 0;
    RogueBehaviorTree* a = factory();
    RogueBehaviorTree* b = factory();
    if (!a || !b)
        return 0;
    uint64_t accumA = 0, accumB = 0;
    for (int i = 0; i < ticks; i++)
    {
        rogue_behavior_tree_tick(a, NULL, 0.016f);
        rogue_behavior_tree_tick(b, NULL, 0.016f);
        uint32_t ha = path_hash(a);
        uint32_t hb = path_hash(b);
        accumA = rogue_fnv1a64(&ha, sizeof ha, accumA ? accumA : 0xcbf29ce484222325ULL);
        accumB = rogue_fnv1a64(&hb, sizeof hb, accumB ? accumB : 0xcbf29ce484222325ULL);
        if (ha != hb)
        {
            rogue_behavior_tree_destroy(a);
            rogue_behavior_tree_destroy(b);
            if (out_hash)
                *out_hash = 0;
            return 0;
        }
    }
    if (out_hash)
        *out_hash = accumA;
    rogue_behavior_tree_destroy(a);
    rogue_behavior_tree_destroy(b);
    return 1;
}
