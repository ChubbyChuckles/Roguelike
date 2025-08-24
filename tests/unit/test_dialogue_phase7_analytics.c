/* Phase 7 analytics test */
#include "../../src/core/dialogue.h"
#include <stdio.h>
#include <string.h>

static const char* script = "npc|Alpha one.\n"
                            "npc|Beta two.\n";

int main(void)
{
    rogue_dialogue_reset();
    if (rogue_dialogue_register_from_buffer(701, script, (int) strlen(script)) != 0)
    {
        printf("FAIL register\n");
        return 1;
    }
    if (rogue_dialogue_start(701) != 0)
    {
        printf("FAIL start\n");
        return 1;
    }
    int lines_viewed = 0;
    double last_ts = 0.0;
    unsigned int digest = 0;
    if (rogue_dialogue_analytics_get(701, &lines_viewed, &last_ts, &digest) != 0)
    {
        printf("FAIL analytics pre\n");
        return 1;
    }
    if (lines_viewed != 1)
    {
        printf("FAIL expected 1 line_viewed got %d\n", lines_viewed);
        return 1;
    }
    unsigned int digest_initial = digest;
    /* Advance (finish) script */
    rogue_dialogue_advance(); /* second line */
    rogue_dialogue_advance(); /* close */
    if (rogue_dialogue_analytics_get(701, &lines_viewed, &last_ts, &digest) != 0)
    {
        printf("FAIL analytics post\n");
        return 1;
    }
    if (lines_viewed != 2)
    {
        printf("FAIL expected 2 lines_viewed got %d\n", lines_viewed);
        return 1;
    }
    if (digest == digest_initial)
    {
        printf("FAIL digest unchanged\n");
        return 1;
    }
    printf("OK test_dialogue_phase7_analytics\n");
    return 0;
}
