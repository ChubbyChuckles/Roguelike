/* Phase 6 typewriter + skip test */
#include "core/dialogue.h"
#include "ui/core/ui_context.h"
#include <stdio.h>
#include <string.h>

static const char* script = "npc|Hello there traveler.\n"
                            "npc|Another line.\n";

int main(void)
{
    rogue_dialogue_reset();
    if (rogue_dialogue_register_from_buffer(601, script, (int) strlen(script)) != 0)
    {
        printf("FAIL register\n");
        return 1;
    }
    rogue_dialogue_typewriter_enable(1, 0.2f); /* 0.2 chars/ms => 5ms per char approx */
    if (rogue_dialogue_start(601) != 0)
    {
        printf("FAIL start\n");
        return 1;
    }
    /* Simulate short updates so first advance should finish reveal not advance line */
    rogue_dialogue_update(20.0); /* reveal ~4 chars */
    const RogueDialoguePlayback* pb = rogue_dialogue_playback();
    if (!pb)
    {
        printf("FAIL playback null\n");
        return 1;
    }
    char buf[128];
    rogue_dialogue_current_text(
        buf, sizeof buf); /* should still return full if we force expand? we rely on current_text
                             ignoring typewriter (it shows full) */
    int r = rogue_dialogue_advance();
    if (r != 2)
    {
        printf("FAIL expected skip-finish got %d\n", r);
        return 1;
    }
    r = rogue_dialogue_advance();
    if (r != 1)
    {
        printf("FAIL expected real advance got %d\n", r);
        return 1;
    }
    /* Simulate enough time to fully reveal second line */
    for (int i = 0; i < 10; i++)
    {
        rogue_dialogue_update(20.0);
    }
    r = rogue_dialogue_advance();
    if (r != 0)
    {
        printf("FAIL expected close got %d\n", r);
        return 1;
    }
    printf("OK test_dialogue_phase6_typewriter\n");
    return 0;
}
