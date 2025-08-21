/* UI Phase 2.1 core primitives test: Image, Sprite, ProgressBar */
#include "ui/core/ui_context.h"
#include <assert.h>
#include <stdio.h>

static void test_phase2_core_primitives()
{
    RogueUIContext ctx;
    RogueUIContextConfig cfg = {.max_nodes = 16, .seed = 7u, .arena_size = 4096};
    assert(rogue_ui_init(&ctx, &cfg));
    rogue_ui_begin(&ctx, 16.6);
    int panel_id = rogue_ui_panel(&ctx, (RogueUIRect){0, 0, 100, 40}, 0x222222FFu);
    assert(panel_id == 0);
    int text_id = rogue_ui_text_dup(&ctx, (RogueUIRect){4, 4, 90, 12}, "Phase2", 0xFFFFFFFFu);
    assert(text_id == 1);
    int img_id = rogue_ui_image(&ctx, (RogueUIRect){10, 20, 32, 32}, "/tmp/icon.png", 0xFFFFFFFFu);
    assert(img_id == 2);
    int spr_id = rogue_ui_sprite(&ctx, (RogueUIRect){50, 20, 32, 32}, 3, 5, 0xFFAAFFFFu);
    assert(spr_id == 3);
    int prog_id = rogue_ui_progress_bar(&ctx, (RogueUIRect){0, 45, 100, 8}, 25.0f, 100.0f,
                                        0x000000FFu, 0x00FF00FFu, 0);
    assert(prog_id == 4);
    int count = 0;
    const RogueUINode* nodes = rogue_ui_nodes(&ctx, &count);
    assert(count == 5);
    assert(nodes[2].kind == 2 && nodes[3].kind == 3 && nodes[4].kind == 4);
    assert(nodes[4].value == 25.0f && nodes[4].value_max == 100.0f &&
           nodes[4].aux_color == 0x00FF00FFu);
    rogue_ui_end(&ctx);
    rogue_ui_shutdown(&ctx);
}

int main()
{
    test_phase2_core_primitives();
    printf("UI Phase2 primitive tests passed\n");
    return 0;
}
