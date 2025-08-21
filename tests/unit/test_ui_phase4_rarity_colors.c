#include "ui/core/ui_context.h"
#include <assert.h>
#include <string.h>

static int count_color(const RogueUINode* nodes, int n, uint32_t color)
{
    int c = 0;
    for (int i = 0; i < n; i++)
        if (nodes[i].color == color)
            c++;
    return c;
}

int main(void)
{
    RogueUIContext ctx;
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 512;
    cfg.seed = 7;
    cfg.arena_size = 8192;
    assert(rogue_ui_init(&ctx, &cfg));
    int ids[8] = {0};
    int counts[8] = {0}; /* Choose ids that produce different rarity (id %5) -> 0..4 */
    ids[0] = 10;
    counts[0] = 1; /* rarity 0 (gold default 240,210,60) */
    ids[1] = 11;
    counts[1] = 2; /* rarity 1 green */
    ids[2] = 12;
    counts[2] = 3; /* rarity 2 blue */
    ids[3] = 13;
    counts[3] = 4; /* rarity 3 purple */
    ids[4] = 14;
    counts[4] = 5; /* rarity 4 orange */
    RogueUIInputState in = {0};
    in.mouse_x = 0;
    in.mouse_y = 0;
    rogue_ui_begin(&ctx, 16.0);
    rogue_ui_set_input(&ctx, &in);
    int fv = 0, vc = 0;
    rogue_ui_inventory_grid(&ctx, (RogueUIRect){0, 0, 200, 64}, "inv", 8, 8, ids, counts, 32, &fv,
                            &vc);
    rogue_ui_end(&ctx);
    int n = 0;
    const RogueUINode* nodes = rogue_ui_nodes(&ctx, &n);
    assert(n > 0);
    /* Expect outer border panels colored per rarity for 5 items present */
    uint32_t col_common = (240u << 24) | (210u << 16) | (60u << 8) | 0xFFu;
    uint32_t col_uncommon = (80u << 24) | (220u << 16) | (80u << 8) | 0xFFu;
    uint32_t col_rare = (80u << 24) | (120u << 16) | (255u << 8) | 0xFFu;
    uint32_t col_epic = (180u << 24) | (70u << 16) | (220u << 8) | 0xFFu;
    uint32_t col_legend = (255u << 24) | (140u << 16) | (0u << 8) | 0xFFu;
    assert(count_color(nodes, n, col_common) >= 1);
    assert(count_color(nodes, n, col_uncommon) >= 1);
    assert(count_color(nodes, n, col_rare) >= 1);
    assert(count_color(nodes, n, col_epic) >= 1);
    assert(count_color(nodes, n, col_legend) >= 1);
    return 0;
}
