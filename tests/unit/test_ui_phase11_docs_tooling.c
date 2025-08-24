#include "../../src/ui/core/ui_context.h"
#include <assert.h>
#include <stdio.h>

/* Phase 11.1/11.2/11.3 tests: style guide build, inspector overlay, crash snapshot */
int main()
{
    RogueUIContextConfig cfg = {.max_nodes = 256, .seed = 7u, .arena_size = 8192};
    RogueUIContext ui;
    assert(rogue_ui_init(&ui, &cfg));
    rogue_ui_begin(&ui, 16.0);
    rogue_ui_style_guide_build(&ui);
    int before = ui.node_count;
    assert(before > 0);
    /* Enable inspector and emit overlays */
    rogue_ui_inspector_enable(&ui, 1);
    rogue_ui_inspector_select(&ui, 0);
    int overlay_last = rogue_ui_inspector_emit(&ui, 0xFFAA10FFu);
    assert(overlay_last >= 0);
    /* Edit color of first node and ensure mutation */
    uint32_t old = ui.nodes[0].color;
    assert(rogue_ui_inspector_edit_color(&ui, 0, 0x12345678u));
    assert(ui.nodes[0].color != old);
    RogueUICrashSnapshot snap = {0};
    assert(rogue_ui_snapshot(&ui, &snap));
    assert(snap.node_count == ui.node_count);
    rogue_ui_end(&ui);
    rogue_ui_shutdown(&ui);
    printf("PHASE11_DOCS_TOOLING_OK nodes=%d overlay_last=%d\n", before, overlay_last);
    return 0;
}
