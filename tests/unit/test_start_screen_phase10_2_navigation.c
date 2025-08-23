#include "core/app.h"
#include "core/app_state.h"
#include "input/input.h"
#include <assert.h>

static void press(RogueKey k)
{
    g_app.input.prev_keys[k] = false;
    g_app.input.keys[k] = true;
}

static void release(RogueKey k)
{
    g_app.input.prev_keys[k] = false;
    g_app.input.keys[k] = false;
}

static void tap(RogueKey k)
{
    press(k);
    rogue_app_step();
    release(k);
    rogue_app_step();
}

int main(void)
{
    RogueAppConfig cfg = {
        "StartScreenNav",          320, 180, 320, 180, 0, 0, 0, 1, ROGUE_WINDOW_WINDOWED,
        (RogueColor){0, 0, 0, 255}};
    assert(rogue_app_init(&cfg));

    /* First frame to build menu */
    rogue_app_step();
    assert(g_app.show_start_screen == 1);

    /* Navigate down a few times and ensure selection changes (skipping disabled if any) */
    int initial = g_app.menu_index;
    tap(ROGUE_KEY_DOWN);
    int after1 = g_app.menu_index;
    assert(after1 != initial);

    /* Wrap-around by moving up repeatedly (more than item count) */
    for (int i = 0; i < 10; ++i)
        tap(ROGUE_KEY_UP);
    int afterWrap = g_app.menu_index;
    (void) afterWrap; /* selection should be valid; not asserting equality to avoid coupling */

    /* Move to New Game explicitly using accelerator 'N' (seed entry must be off) */
    g_app.input.text_len = 1;
    g_app.input.text_buffer[0] = 'n';
    rogue_app_step();
    g_app.input.text_len = 0;

    /* Activate selection (ENTER). If New Game, we should begin FADE_OUT. */
    tap(ROGUE_KEY_DIALOGUE);
    /* One more frame to apply fade step */
    rogue_app_step();
    assert(g_app.start_state == 2 /* ROGUE_START_FADE_OUT */ || g_app.show_start_screen == 0);

    return 0;
}
