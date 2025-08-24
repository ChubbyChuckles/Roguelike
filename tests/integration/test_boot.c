#include "../../src/core/app/app.h"
#include "../../src/game/game_loop.h"
#include <stdio.h>

int main(void)
{
    RogueAppConfig cfg = {"TestBoot", 160, 90, 0};
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
        return 1;
    }
    rogue_game_loop_request_exit();
    rogue_app_shutdown();
    return 0;
}
