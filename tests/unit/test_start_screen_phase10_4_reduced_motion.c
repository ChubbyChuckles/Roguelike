#include "../../src/core/app.h"
#include "../../src/core/app_state.h"
#include <assert.h>

static unsigned int quant_fade(void)
{
    return (unsigned int) (g_app.start_state_t * 1000.0f + 0.5f);
}

int main(void)
{
    RogueAppConfig cfg = {
        "StartScreenReducedMotion", 320, 180, 320, 180, 0, 0, 0, 1, ROGUE_WINDOW_WINDOWED,
        (RogueColor){0, 0, 0, 255}};
    assert(rogue_app_init(&cfg));

    /* Enable reduced motion and step; fade should be skipped to MENU with t=1 */
    g_app.reduced_motion = 1;
    rogue_app_step();
    unsigned int t = quant_fade();
    if (!(g_app.start_state == 1 /* MENU */ && t == 1000))
        return 1;

    /* Trigger fade out and verify it completes quickly next frame too */
    g_app.start_state = 2; /* FADE_OUT */
    rogue_app_step();
    if (!(g_app.show_start_screen == 0 || g_app.start_state_t == 0))
        return 1;

    rogue_app_shutdown();
    return 0;
}
