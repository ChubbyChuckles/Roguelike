#include "../../src/core/app/app.h"
#include "../../src/game/game_loop.h"
#include <stdio.h>

int main(void)
{
#if defined(_WIN32)
    _chdir("..\\..\\");
#else
    chdir("../..");
#endif
    /* Use full config matching current RogueAppConfig layout for stability */
    RogueAppConfig cfg = {"TestBoot",
                          320,
                          180,
                          320,
                          180,
                          0,
                          0,
                          0,
                          1,
                          ROGUE_WINDOW_WINDOWED,
                          (RogueColor){0, 0, 0, 255}};
    if (!rogue_app_init(&cfg))
    {
        printf("init fail\n");
        return 1;
    }
    int before = rogue_app_frame_count();
    for (int i = 0; i < 5; ++i)
        rogue_app_step();
    int after = rogue_app_frame_count();
    if (after - before < 5)
    {
        printf("frame count fail\n");
        rogue_game_loop_request_exit();
        rogue_app_shutdown();
        return 1;
    }
    rogue_game_loop_request_exit();
    rogue_app_shutdown();
    return 0;
}
