#define SDL_MAIN_HANDLED 1
#include "ui/core/ui_context.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Tests Phases 5.4 - 5.7: synergy panel toggle (presence heuristic), tag filtering, export/import,
 * allocate + undo */

static void populate(RogueUIContext* ui)
{
    rogue_ui_skillgraph_begin(ui, 0, 0, 300, 200, 1.0f);
    /* icon_id, ranks vary; assign tags */
    for (int i = 0; i < 10; i++)
    {
        unsigned int tags = 0;
        if (i % 2 == 0)
            tags |= ROGUE_UI_SKILL_TAG_FIRE;
        if (i % 3 == 0)
            tags |= ROGUE_UI_SKILL_TAG_MOVEMENT;
        if (i % 4 == 0)
            tags |= ROGUE_UI_SKILL_TAG_DEFENSE;
        int synergy = (i % 5) == 0;
        rogue_ui_skillgraph_add(ui, (float) (i * 25), (float) ((i % 5) * 32), i, i % 3, 3, synergy,
                                tags);
    }
}

int main()
{
    RogueUIContext ui;
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 1024;
    cfg.seed = 11;
    cfg.arena_size = 24 * 1024;
    if (!rogue_ui_init(&ui, &cfg))
    {
        printf("FAIL init\n");
        return 1;
    }
    int fails = 0;
    /* Build with all nodes */
    rogue_ui_begin(&ui, 16.0);
    populate(&ui);
    rogue_ui_skillgraph_build(&ui);
    int count_all = 0;
    const RogueUINode* nodes_all = rogue_ui_nodes(&ui, &count_all);
    rogue_ui_end(&ui);
    if (count_all <= 0)
    {
        printf("FAIL no nodes emitted_all=%d\n", count_all);
        fails++;
    }
    int base_icons_all = 0;
    for (int i = 0; i < count_all; i++)
    {
        if (nodes_all[i].kind == 3 /* sprite */)
            base_icons_all++;
    }

    /* Apply tag filter (FIRE only) */
    rogue_ui_skillgraph_set_filter_tags(&ui, ROGUE_UI_SKILL_TAG_FIRE);
    rogue_ui_begin(&ui, 16.0);
    populate(&ui);
    rogue_ui_skillgraph_build(&ui);
    int count_fire = 0;
    const RogueUINode* nodes_fire = rogue_ui_nodes(&ui, &count_fire);
    rogue_ui_end(&ui);
    int base_icons_fire = 0;
    for (int i = 0; i < count_fire; i++)
    {
        if (nodes_fire[i].kind == 3)
            base_icons_fire++;
    }
    if (base_icons_fire <= 0 || base_icons_fire >= base_icons_all)
    {
        printf("FAIL tag filter ineffective base_all=%d base_fire=%d\n", base_icons_all,
               base_icons_fire);
        fails++;
    }

    /* Export current build */
    char buf[512];
    size_t n = rogue_ui_skillgraph_export(&ui, buf, sizeof buf);
    if (n == 0)
    {
        printf("FAIL export empty\n");
        fails++;
    }

    /* Allocate rank on icon 2 until max then undo one */
    int allocs = 0;
    while (rogue_ui_skillgraph_allocate(&ui, 2))
        allocs++;
    if (allocs == 0)
    {
        printf("FAIL allocate none\n");
        fails++;
    }
    if (!rogue_ui_skillgraph_undo(&ui))
    {
        printf("FAIL undo failed\n");
        fails++;
    }

    /* Modify export buffer rank for icon 3 to 2 (ensure <= max) and re-import */
    /* Find line starting with '3:' */
    char* line = strstr(buf, "3:");
    if (line)
    {
        char* slash = strchr(line, '/');
        if (slash)
        {
            line[2] = '2'; /* line format d: r / m ; tags -> set rank char position (icon_id single
                              digit assumption) */
        }
    }
    int applied = rogue_ui_skillgraph_import(&ui, buf);
    if (applied <= 0)
    {
        printf("FAIL import applied=%d\n", applied);
        fails++;
    }

    /* Clear filter and rebuild */
    rogue_ui_skillgraph_set_filter_tags(&ui, 0);
    rogue_ui_begin(&ui, 16.0);
    populate(&ui);
    int emitted_final = rogue_ui_skillgraph_build(&ui);
    rogue_ui_end(&ui);
    if (emitted_final <= 0)
    {
        printf("FAIL final emitted=0\n");
        fails++;
    }

    if (fails == 0)
        printf("test_ui_phase5_skillgraph_advanced: OK (base_all=%d base_fire=%d final_nodes=%d "
               "export_bytes=%zu allocs=%d applied=%d)\n",
               base_icons_all, base_icons_fire, emitted_final, n, allocs, applied);
    rogue_ui_shutdown(&ui);
    return fails ? 1 : 0;
}
