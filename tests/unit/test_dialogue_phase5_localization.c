/* Phase 5 localization test: register lines with keys and switch locales */
#include "core/dialogue.h"
#include <stdio.h>
#include <string.h>

static const char* script = "npc|[greeting]Hello there.\n"
                            "npc|[farewell]Goodbye.\n";

int main(void)
{
    rogue_dialogue_reset();
    if (rogue_dialogue_register_from_buffer(501, script, (int) strlen(script)) != 0)
    {
        printf("FAIL register\n");
        return 1;
    }
    /* Register two locales */
    if (rogue_dialogue_locale_register("en", "greeting", "Hello there.") != 0)
    {
        printf("FAIL reg en greeting\n");
        return 1;
    }
    if (rogue_dialogue_locale_register("en", "farewell", "Goodbye.") != 0)
    {
        printf("FAIL reg en farewell\n");
        return 1;
    }
    if (rogue_dialogue_locale_register("fr", "greeting", "Bonjour.") != 0)
    {
        printf("FAIL reg fr greeting\n");
        return 1;
    }
    if (rogue_dialogue_locale_register("fr", "farewell", "Au revoir.") != 0)
    {
        printf("FAIL reg fr farewell\n");
        return 1;
    }
    if (rogue_dialogue_start(501) != 0)
    {
        printf("FAIL start\n");
        return 1;
    }
    char buf[128];
    if (rogue_dialogue_current_text(buf, sizeof buf) <= 0)
    {
        printf("FAIL read en line0\n");
        return 1;
    }
    if (strstr(buf, "Hello there.") == NULL)
    {
        printf("FAIL expected en greeting got %s\n", buf);
        return 1;
    }
    if (rogue_dialogue_advance() != 1)
    {
        printf("FAIL advance to farewell\n");
        return 1;
    }
    if (rogue_dialogue_current_text(buf, sizeof buf) <= 0)
    {
        printf("FAIL read en line1\n");
        return 1;
    }
    if (strstr(buf, "Goodbye.") == NULL)
    {
        printf("FAIL expected en farewell got %s\n", buf);
        return 1;
    }
    /* Switch locale mid-script */
    if (rogue_dialogue_locale_set("fr") != 0)
    {
        printf("FAIL set fr\n");
        return 1;
    }
    if (rogue_dialogue_current_text(buf, sizeof buf) <= 0)
    {
        printf("FAIL read fr line1\n");
        return 1;
    }
    if (strstr(buf, "Au revoir.") == NULL)
    {
        printf("FAIL expected fr farewell got %s\n", buf);
        return 1;
    }
    /* Restart to verify first line in FR */
    if (rogue_dialogue_start(501) != 0)
    {
        printf("FAIL restart\n");
        return 1;
    }
    if (rogue_dialogue_current_text(buf, sizeof buf) <= 0)
    {
        printf("FAIL read fr line0 restart\n");
        return 1;
    }
    if (strstr(buf, "Bonjour.") == NULL)
    {
        printf("FAIL expected fr greeting got %s\n", buf);
        return 1;
    }
    printf("OK test_dialogue_phase5_localization\n");
    return 0;
}
