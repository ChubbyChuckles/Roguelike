#include "ui/core/ui_context.h"
#include "ui/core/ui_test_harness.h"
#include "ui/core/ui_theme.h"
#include <assert.h>
#include <stdio.h>

static void build_simple(RogueUIContext* ctx)
{
    rogue_ui_panel(ctx, (RogueUIRect){0, 0, 100, 40}, 0x202028FFu);
    rogue_ui_text(ctx, (RogueUIRect){4, 4, 92, 12}, "Theme", 0xFFFFFFFFu);
}

int main()
{
    RogueUIContext ctx;
    memset(&ctx, 0, sizeof ctx);
    RogueUIContextConfig cfg = {.max_nodes = 32, .seed = 77u};
    assert(rogue_ui_init(&ctx, &cfg));
    fprintf(stderr, "GOLDEN: after init cap=%d nodes_ptr=%p arena=%p\n", ctx.node_capacity,
            (void*) ctx.nodes, (void*) ctx.arena);
    rogue_ui_begin(&ctx, 16.0);
    build_simple(&ctx);
    rogue_ui_end(&ctx);
    fprintf(stderr, "GOLDEN: after first frame node_count=%d\n", ctx.node_count);
    RogueUIDrawSample baseline[32];
    size_t base_ct = rogue_ui_draw_capture(&ctx, baseline, 32);
    uint64_t hash_before = rogue_ui_tree_hash(&ctx);
    fprintf(stderr, "GOLDEN: shutting down first ctx nodes_ptr=%p arena=%p\n", (void*) ctx.nodes,
            (void*) ctx.arena);
    rogue_ui_shutdown(&ctx);
    fprintf(stderr, "GOLDEN: after first shutdown nodes_ptr=%p arena=%p\n", (void*) ctx.nodes,
            (void*) ctx.arena);
    // Rebuild second time (should match)
    assert(rogue_ui_init(&ctx, &cfg));
    fprintf(stderr, "GOLDEN: after reinit cap=%d nodes_ptr=%p arena=%p\n", ctx.node_capacity,
            (void*) ctx.nodes, (void*) ctx.arena);
    rogue_ui_begin(&ctx, 16.0);
    build_simple(&ctx);
    rogue_ui_end(&ctx);
    fprintf(stderr, "GOLDEN: after second frame node_count=%d\n", ctx.node_count);
    int changed = 0;
    assert(rogue_ui_golden_within_tolerance(&ctx, baseline, base_ct, 0, &changed));
    assert(changed == 0);
    uint64_t hash_after = rogue_ui_tree_hash(&ctx);
    assert(hash_before == hash_after);
    // Theme diff test: modify one field (panel_bg) and confirm diff bit set
    RogueUIThemePack a = {0};
    a.panel_bg = 0x111111FFu;
    a.panel_border = 1;
    a.text_normal = 2;
    a.text_accent = 3;
    a.button_bg = 4;
    a.button_bg_hot = 5;
    a.button_text = 6;
    a.slider_track = 7;
    a.slider_fill = 8;
    a.tooltip_bg = 9;
    a.alert_text = 10;
    a.font_size_base = 12;
    a.padding_small = 2;
    a.padding_large = 4;
    a.dpi_scale_x100 = 100;
    RogueUIThemePack b = a;
    b.panel_bg = 0x222222FFu;
    unsigned int diff = rogue_ui_theme_diff(&a, &b);
    assert(diff & 1u); // first field changed
    fprintf(stderr, "GOLDEN: shutting down final ctx nodes_ptr=%p arena=%p\n", (void*) ctx.nodes,
            (void*) ctx.arena);
    rogue_ui_shutdown(&ctx);
    fprintf(stderr, "GOLDEN: after final shutdown nodes_ptr=%p arena=%p\n", (void*) ctx.nodes,
            (void*) ctx.arena);
    printf("PHASE10_GOLDEN_THEME_OK baseline=%zu hash=%llu diff_mask=%u\n", base_ct,
           (unsigned long long) hash_after, diff);
    return 0;
}
