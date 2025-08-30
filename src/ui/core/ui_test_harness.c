/**
 * @file ui_test_harness.c
 * @brief UI testing harness providing golden master testing, fuzz testing,
 *        and performance benchmarking for the roguelike UI system.
 */

#include "ui_test_harness.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Captures UI draw samples from a UI context for testing purposes.
 *
 * @param ctx The UI context to capture from.
 * @param out Array to store the captured draw samples.
 * @param max Maximum number of samples to capture.
 * @return Number of samples actually captured.
 */
size_t rogue_ui_draw_capture(const RogueUIContext* ctx, RogueUIDrawSample* out, size_t max)
{
    if (!ctx || !out || max == 0)
        return 0;
    size_t n = (size_t) ctx->node_count;
    if (n > max)
        n = max;
    for (size_t i = 0; i < n; i++)
    {
        out[i].x = ctx->nodes[i].rect.x;
        out[i].y = ctx->nodes[i].rect.y;
        out[i].w = ctx->nodes[i].rect.w;
        out[i].h = ctx->nodes[i].rect.h;
        out[i].color = ctx->nodes[i].color;
        out[i].kind = ctx->nodes[i].kind;
    }
    return n;
}

/**
 * @brief Compares current UI state against a golden master baseline.
 *
 * @param ctx The current UI context to compare.
 * @param baseline Array of baseline draw samples.
 * @param baseline_count Number of samples in the baseline.
 * @param out_changed Optional output parameter for number of changed samples.
 * @return Number of differences found, or -1 on error.
 */
int rogue_ui_golden_diff(const RogueUIContext* ctx, const RogueUIDrawSample* baseline,
                         size_t baseline_count, int* out_changed)
{
    if (out_changed)
        *out_changed = 0;
    if (!ctx || !baseline)
        return -1;
    size_t cur_count = (size_t) ctx->node_count;
    size_t compare = cur_count < baseline_count ? cur_count : baseline_count;
    int changed = 0;
    for (size_t i = 0; i < compare; i++)
    {
        const RogueUINode* n = &ctx->nodes[i];
        const RogueUIDrawSample* b = &baseline[i];
        if (n->rect.x != b->x || n->rect.y != b->y || n->rect.w != b->w || n->rect.h != b->h ||
            n->color != b->color || n->kind != b->kind)
        {
            changed++;
        }
    }
    if (cur_count != baseline_count)
        changed += (int) (cur_count > baseline_count ? cur_count - baseline_count
                                                     : baseline_count - cur_count);
    if (out_changed)
        *out_changed = changed;
    return changed;
}

/**
 * @brief Checks if UI state is within tolerance of a golden master baseline.
 *
 * @param ctx The current UI context to check.
 * @param baseline Array of baseline draw samples.
 * @param baseline_count Number of samples in the baseline.
 * @param tolerance Maximum allowed number of differences.
 * @param out_changed Optional output parameter for actual number of changes.
 * @return 1 if within tolerance, 0 otherwise.
 */
int rogue_ui_golden_within_tolerance(const RogueUIContext* ctx, const RogueUIDrawSample* baseline,
                                     size_t baseline_count, int tolerance, int* out_changed)
{
    int diff = rogue_ui_golden_diff(ctx, baseline, baseline_count, out_changed);
    return diff <= tolerance;
}

/**
 * @brief Simple PRNG for generating test data in fuzz testing.
 *
 * @return Next random unsigned integer.
 */
static unsigned int harness_prng_state = 0xA5F15327u;
static unsigned int prng_next()
{
    unsigned int x = harness_prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    harness_prng_state = x;
    return x;
}

/**
 * @brief Performs fuzz testing on UI grid layout to detect layout violations.
 *
 * This function generates random grid layouts and checks that all cells remain
 * within their parent container bounds.
 *
 * @param iterations Number of fuzz test iterations to run.
 * @return Number of layout violations detected, or -1 on initialization error.
 */
int rogue_ui_layout_fuzz(int iterations)
{
    RogueUIContext ctx;
    RogueUIContextConfig cfg = {.max_nodes = 256, .seed = 1234u};
    int violations = 0;
    for (int it = 0; it < iterations; ++it)
    {
        if (!rogue_ui_init(&ctx, &cfg))
            return -1;
        rogue_ui_begin(&ctx, 16.0);
        RogueUIRect root = {0, 0, 300, 200};
        int root_idx = rogue_ui_panel(&ctx, root, 0x101010FFu);
        (void) root_idx;
        int rows = 2 + (int) (prng_next() % 3);
        int cols = 2 + (int) (prng_next() % 3);
        int padding = 4;
        int spacing = 2; /* generate grid of buttons */
        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                RogueUIRect cell = rogue_ui_grid_cell(root, rows, cols, r, c, padding, spacing);
                rogue_ui_panel(&ctx, cell, 0x202020FFu); /* property invariant: cell inside root */
                if (cell.x < root.x || cell.y < root.y ||
                    cell.x + cell.w > root.x + root.w + 0.01f ||
                    cell.y + cell.h > root.y + root.h + 0.01f)
                {
                    violations++;
                }
            }
        }
        rogue_ui_end(&ctx);
        rogue_ui_shutdown(&ctx);
        if (violations > 0)
            break;
    }
    return violations;
}

/**
 * @brief Performance test that builds many UI panels to measure rendering throughput.
 *
 * @param ctx The UI context to build panels in.
 * @param count Number of panels to attempt to build.
 * @return Number of panels actually emitted before hitting capacity limits.
 */
int rogue_ui_perf_build_many(RogueUIContext* ctx, int count)
{
    if (!ctx || count <= 0)
        return 0;
    int emitted = 0;
    for (int i = 0; i < count; i++)
    {
        float x = (float) (i % 64) * 10.0f;
        float y = (float) (i / 64) * 10.0f;
        if (rogue_ui_panel(ctx, (RogueUIRect){x, y, 8, 8}, 0x303030FFu) >= 0)
            emitted++;
        if (ctx->node_count >= ctx->node_capacity)
            break;
    }
    return emitted;
}
