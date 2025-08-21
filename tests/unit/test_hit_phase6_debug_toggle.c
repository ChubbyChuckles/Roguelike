#define SDL_MAIN_HANDLED 1
#include "game/hit_system.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    /* Ensure toggle changes global flag and that store preserves state */
    assert(g_hit_debug_enabled == 0);
    rogue_hit_debug_toggle(1);
    assert(g_hit_debug_enabled == 1);
    rogue_hit_debug_toggle(0);
    assert(g_hit_debug_enabled == 0);
    printf("hit_phase6_debug_toggle_ok\n");
    return 0;
}
