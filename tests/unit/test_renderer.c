#include "graphics/renderer.h"
#include <stdio.h>

int main(void)
{
    RogueRenderer r;
    if (!rogue_renderer_init(&r))
    {
        printf("renderer init fail\n");
        return 1;
    }
    RogueColor c = {255, 0, 0, 255};
    rogue_renderer_set_draw_color(&r, c);
    rogue_renderer_clear(&r);
    rogue_renderer_present(&r);
    rogue_renderer_shutdown(&r);
    return 0;
}
