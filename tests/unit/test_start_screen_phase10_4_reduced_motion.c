#include "../../src/core/app/app.h"
#include "../../src/core/app/app_state.h"
#include <assert.h>
#include <stdio.h>

static unsigned int quant_fade(void)
{
    return (unsigned int) (g_app.start_state_t * 1000.0f + 0.5f);
}

static void dump_state(const char* tag)
{
    printf("[%s] reduced_motion=%d start_state=%d t=%.3f show=%d\n", tag, g_app.reduced_motion,
           g_app.start_state, (double) g_app.start_state_t, g_app.show_start_screen);
    fflush(stdout);
}

int main(void)
{
    RogueAppConfig cfg = {
        "StartScreenReducedMotion", 320, 180, 320, 180, 0, 0, 0, 1, ROGUE_WINDOW_WINDOWED,
        (RogueColor){0, 0, 0, 255}};
    assert(rogue_app_init(&cfg));

    /* Enable reduced motion and step; fade should be skipped to MENU with t=1 */
    g_app.reduced_motion = 1;
    dump_state("before_step1");
    rogue_app_step();
    dump_state("after_step1");
    unsigned int t = quant_fade();
    if (!(g_app.start_state == 1 /* MENU */ && t == 1000))
    {
        printf("FAIL phase=fade_in_skip expected MENU/t=1.000 got state=%d t=%u\n",
               g_app.start_state, t);
        rogue_app_shutdown();
        return 1;
    }

    /* Trigger fade out and verify it completes quickly next frame too */
    g_app.start_state = 2; /* FADE_OUT */
    dump_state("before_step2");
    rogue_app_step();
    dump_state("after_step2");
    if (!(g_app.show_start_screen == 0 || g_app.start_state_t == 0))
    {
        printf("FAIL phase=fade_out_skip expected hide OR t=0 got show=%d t=%.3f\n",
               g_app.show_start_screen, (double) g_app.start_state_t);
        rogue_app_shutdown();
        return 1;
    }

    rogue_app_shutdown();
    return 0;
}
