#include "../../src/ui/core/ui_context.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 256;
    cfg.arena_size = 4096; /* seed left 0 */
    RogueUIContext ctx;
    assert(rogue_ui_init(&ctx, &cfg));

    RogueUIInputState in = {0};
    rogue_ui_set_input(&ctx, &in);
    rogue_ui_begin(&ctx, 0.0);
    RogueUIRect row_r = {0, 0, 300, 40};
    int row = rogue_ui_row_begin(&ctx, row_r, 4, 2);
    RogueUIRect r1;
    assert(rogue_ui_row_next(&ctx, row, 50, 30, &r1));
    int b1 = rogue_ui_button(&ctx, r1, "BtnA", 0x111111u, 0xffffffu);
    assert(b1 >= 0);
    RogueUIRect r2;
    assert(rogue_ui_row_next(&ctx, row, 60, 30, &r2));
    int b2 = rogue_ui_button(&ctx, r2, "BtnB", 0x222222u, 0xffffffu);
    assert(b2 >= 0);
    assert(ctx.nodes[b1].rect.x < ctx.nodes[b2].rect.x);
    assert(ctx.nodes[b1].id_hash == rogue_ui_make_id("BtnA"));
    assert(rogue_ui_find_by_id(&ctx, ctx.nodes[b2].id_hash) == &ctx.nodes[b2]);

    RogueUIRect col_r = {0, 50, 100, 200};
    int col = rogue_ui_column_begin(&ctx, col_r, 5, 3);
    RogueUIRect c1;
    assert(rogue_ui_column_next(&ctx, col, 80, 20, &c1));
    int tgl_state = 0;
    int t1 = rogue_ui_toggle(&ctx, c1, "Tog1", &tgl_state, 0x0, 0x0, 0xffffffu);
    assert(t1 >= 0);
    RogueUIRect c2;
    assert(rogue_ui_column_next(&ctx, col, 80, 20, &c2));
    float val = 0.5f;
    int s1 = rogue_ui_slider(&ctx, c2, 0, 1, &val, 0x0, 0x0);
    assert(s1 >= 0);
    assert(ctx.nodes[t1].rect.y < ctx.nodes[s1].rect.y);

    RogueUIRect grid = {200, 200, 120, 120};
    RogueUIRect cell = rogue_ui_grid_cell(grid, 2, 2, 1, 1, 4, 2);
    assert(cell.w > 0 && cell.h > 0);

    rogue_ui_end(&ctx);

    // second frame stable IDs
    rogue_ui_set_input(&ctx, &in);
    rogue_ui_begin(&ctx, 0.0);
    int b1_second = rogue_ui_button(&ctx, r1, "BtnA", 0, 0); // repeat
    int b2_second = rogue_ui_button(&ctx, r2, "BtnB", 0, 0);
    assert(b1_second >= 0 && b2_second >= 0);
    uint32_t id_btnA = rogue_ui_make_id("BtnA");
    const RogueUINode* found = rogue_ui_find_by_id(&ctx, id_btnA);
    assert(found && found->id_hash == id_btnA);
    rogue_ui_end(&ctx);

    return 0;
}
