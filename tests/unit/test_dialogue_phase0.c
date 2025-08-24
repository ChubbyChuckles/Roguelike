/* test_dialogue_phase0.c - Phase 0 Dialogue System (data model + loader + registry) */
#include "../../src/game/dialogue.h"
#include <stdio.h>
#include <string.h>

static const char* sample = "hero|Hello there adventurer!\n"
                            "npc|Welcome to the village.\n"
                            "hero|Farewell.\n";

int main(void)
{
    rogue_dialogue_reset();
    if (rogue_dialogue_register_from_buffer(101, sample, (int) strlen(sample)) != 0)
    {
        printf("FAIL dialogue register\n");
        return 1;
    }
    if (rogue_dialogue_script_count() != 1)
    {
        printf("FAIL script count\n");
        return 1;
    }
    const RogueDialogueScript* sc = rogue_dialogue_get(101);
    if (!sc)
    {
        printf("FAIL get script\n");
        return 1;
    }
    if (sc->line_count != 3)
    {
        printf("FAIL expected 3 lines got %d\n", sc->line_count);
        return 1;
    }
    if (strcmp(sc->lines[0].speaker_id, "hero") != 0)
    {
        printf("FAIL speaker0\n");
        return 1;
    }
    if (strcmp(sc->lines[1].speaker_id, "npc") != 0)
    {
        printf("FAIL speaker1\n");
        return 1;
    }
    if (strstr(sc->lines[2].text, "Farewell") == NULL)
    {
        printf("FAIL line2 text\n");
        return 1;
    }
    /* Duplicate id rejection */
    if (rogue_dialogue_register_from_buffer(101, sample, (int) strlen(sample)) == 0)
    {
        printf("FAIL duplicate id allowed\n");
        return 1;
    }
    printf("OK test_dialogue_phase0 (%d lines)\n", sc->line_count);
    return 0;
}
