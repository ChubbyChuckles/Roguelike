#include "../../src/core/app.h"
#include "../../src/core/game_loop.h"
#include <stdio.h>

int main(void)
{
#if defined(_WIN32)
    _chdir("..\\..\\");
#else
    chdir("../..\");
#endif
    RogueAppConfig cfg = {"SpawnTest", 160, 90, 0};
    if (!rogue_app_init(&cfg))
    {
        printf("init fail\n");
        return 1;
    }
    rogue_app_skip_start_screen();
    int initial = rogue_app_enemy_count();
    /* Advance enough frames for spawn system to run several times */
    for (int i = 0; i < 200; ++i)
        rogue_app_step();
    int after = rogue_app_enemy_count();
    if (after <= initial)
    {
        printf("enemy spawn fail: initial=%d after=%d\n", initial, after);
        rogue_game_loop_request_exit();
        rogue_app_shutdown();
        return 1;
    }
    rogue_game_loop_request_exit();
    rogue_app_shutdown();
    return 0;
}
