/* test_dialogue_phase3_effects.c - Phase 3 effects execution + idempotence */
#include "../../src/core/dialogue.h"
#include "../../src/ui/core/ui_context.h"
#include <stdio.h>
#include <string.h>

/* Script: three lines, first sets a flag, second gives item, third both. */
static const char* sample = "npc|Welcome hero.|SET_FLAG(intro_seen)\n"
                            "npc|Take this.|GIVE_ITEM(2001,3)\n"
                            "npc|Quest start now.|SET_FLAG(quest_started)|GIVE_ITEM(2001,2)\n";

int main(void)
{
    rogue_dialogue_reset();
    if (rogue_dialogue_register_from_buffer(90, sample, (int) strlen(sample)) != 0)
    {
        printf("FAIL register\n");
        return 1;
    }
    if (rogue_dialogue_start(90) != 0)
    {
        printf("FAIL start\n");
        return 1;
    }
    RogueUIContext ui = {0};
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 128;
    cfg.seed = 42;
    cfg.arena_size = 4096;
    rogue_ui_init(&ui, &cfg);
    for (int i = 0; i < 3; i++)
    {
        if (!rogue_dialogue_playback())
        {
            printf("FAIL playback inactive line %d\n", i);
            rogue_ui_shutdown(&ui);
            return 1;
        }
        rogue_ui_begin(&ui, 16.0);
        /* First render executes */
        rogue_dialogue_render_ui(&ui);
        int flags_before = rogue_dialogue_effect_flag_count();
        int items_before = rogue_dialogue_effect_item_count();
        /* Second render same frame should not change counts */
        rogue_dialogue_render_ui(&ui);
        rogue_ui_end(&ui);
        if (flags_before != rogue_dialogue_effect_flag_count())
        {
            printf("FAIL flag idempotence line %d\n", i);
            rogue_ui_shutdown(&ui);
            return 1;
        }
        if (items_before != rogue_dialogue_effect_item_count())
        {
            printf("FAIL item idempotence line %d\n", i);
            rogue_ui_shutdown(&ui);
            return 1;
        }
        if (i < 2)
        {
            if (rogue_dialogue_advance() != 1)
            {
                printf("FAIL advance line %d\n", i);
                rogue_ui_shutdown(&ui);
                return 1;
            }
        }
        else
        {
            if (rogue_dialogue_advance() != 0)
            {
                printf("FAIL final close\n");
                rogue_ui_shutdown(&ui);
                return 1;
            }
        }
    }
    rogue_ui_shutdown(&ui);
    if (rogue_dialogue_effect_flag_count() != 2)
    {
        printf("FAIL expected 2 flags got %d\n", rogue_dialogue_effect_flag_count());
        return 1;
    }
    if (strcmp(rogue_dialogue_effect_flag(0), "intro_seen") != 0)
    {
        printf("FAIL flag0\n");
        return 1;
    }
    if (strcmp(rogue_dialogue_effect_flag(1), "quest_started") != 0)
    {
        printf("FAIL flag1\n");
        return 1;
    }
    if (rogue_dialogue_effect_item_count() != 2)
    {
        printf("FAIL expected 2 item grant entries got %d\n", rogue_dialogue_effect_item_count());
        return 1;
    }
    int id0, qty0, id1, qty1;
    rogue_dialogue_effect_item(0, &id0, &qty0);
    rogue_dialogue_effect_item(1, &id1, &qty1);
    if (id0 != 2001 || qty0 != 3)
    {
        printf("FAIL item0 %d %d\n", id0, qty0);
        return 1;
    }
    if (id1 != 2001 || qty1 != 2)
    {
        printf("FAIL item1 %d %d\n", id1, qty1);
        return 1;
    }
    printf("OK test_dialogue_phase3_effects\n");
    return 0;
}
