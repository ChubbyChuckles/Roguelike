#include "../../src/core/app/app.h"
#include "../../src/core/app/app_state.h"
#include "../../src/input/input.h"
#include <assert.h>
#include <stdio.h>

static void tap(RogueKey k)
{
    g_app.input.prev_keys[k] = false;
    g_app.input.keys[k] = true;
    rogue_app_step();
    g_app.input.keys[k] = false;
    rogue_app_step();
}

int main(void)
{
    /* Create a corrupt/short save header for slot 0 before init */
    {
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, "save_slot_0.sav", "wb");
#else
        f = fopen("save_slot_0.sav", "wb");
#endif
        if (!f)
            return 1;
        unsigned char junk[4] = {0xDE, 0xAD, 0xBE, 0xEF};
        fwrite(junk, 1, sizeof junk, f);
        fclose(f);
    }

    RogueAppConfig cfg = {
        "StartScreenCorrupt",      320, 180, 320, 180, 0, 0, 0, 1, ROGUE_WINDOW_WINDOWED,
        (RogueColor){0, 0, 0, 255}};
    assert(rogue_app_init(&cfg));

    /* First frame updates the start screen and scans saves */
    rogue_app_step();
    assert(g_app.show_start_screen == 1);

    /* Attempt Continue (should be disabled; activation ignored) */
    g_app.menu_index = 0; /* Continue */
    tap(ROGUE_KEY_DIALOGUE);
    /* After tap, start_state should not be FADE_OUT (2) */
    assert(!(g_app.start_state == 2 && g_app.show_start_screen == 0));

    /* Open Load overlay then immediately close (no valid entries) */
    g_app.menu_index = 2; /* Load */
    tap(ROGUE_KEY_DIALOGUE);
    /* One frame after, overlay should have closed if there were no valid slots */
    rogue_app_step();
    assert(g_app.start_show_load_list == 0);

    /* Cleanup test artifact */
#if defined(_MSC_VER)
    _unlink("save_slot_0.sav");
#else
    remove("save_slot_0.sav");
#endif

    rogue_app_shutdown();
    return 0;
}
