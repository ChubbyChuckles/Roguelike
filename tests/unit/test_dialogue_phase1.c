/* test_dialogue_phase1.c - Phase 1 playback + UI integration unit test (headless UI) */
#include "core/dialogue.h"
#include "ui/core/ui_context.h"
#include <stdio.h>
#include <string.h>

static const char* sample = "narrator|Line one.\n"
                            "narrator|Line two.\n"
                            "narrator|Line three.\n";

int main(void)
{
    RogueUIContext ui = {0};
    RogueUIContextConfig cfg = {.max_nodes = 256, .seed = 1234u, .arena_size = 4096};
    if (rogue_ui_init(&ui, &cfg) == 0)
    {
        printf("FAIL ui init\n");
        return 1;
    }

    rogue_dialogue_reset();
    if (rogue_dialogue_register_from_buffer(7, sample, (int) strlen(sample)) != 0)
    {
        printf("FAIL register\n");
        return 1;
    }
    if (rogue_dialogue_start(7) != 0)
    {
        printf("FAIL start\n");
        return 1;
    }
    const RogueDialoguePlayback* pb = rogue_dialogue_playback();
    if (!pb || pb->line_index != 0)
    {
        printf("FAIL playback start index\n");
        return 1;
    }

    /* Simulate frames and advances */
    for (int i = 0; i < 3; i++)
    {
        rogue_ui_begin(&ui, 16.0);
        rogue_dialogue_update(16.0);
        rogue_dialogue_render_ui(&ui);
        rogue_ui_end(&ui);
        if (i < 2)
        {
            if (rogue_dialogue_advance() != 1)
            {
                printf("FAIL advance mid %d\n", i);
                return 1;
            }
        }
        else
        {
            if (rogue_dialogue_advance() != 0)
            {
                printf("FAIL final close\n");
                return 1;
            }
        }
    }

    if (rogue_dialogue_playback() != NULL)
    {
        printf("FAIL playback not closed\n");
        return 1;
    }
    rogue_ui_shutdown(&ui);
    printf("OK test_dialogue_phase1\n");
    return 0;
}
