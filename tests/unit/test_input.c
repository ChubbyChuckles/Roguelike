#include "input/input.h"
#include <stdio.h>

int main(void)
{
    RogueInputState st;
    rogue_input_clear(&st);
    if (rogue_input_is_down(&st, ROGUE_KEY_UP))
    {
        printf("clear fail\n");
        return 1;
    }
    rogue_input_apply_direction(&st, 0, -1);
    if (!rogue_input_is_down(&st, ROGUE_KEY_UP) || rogue_input_is_down(&st, ROGUE_KEY_DOWN))
    {
        printf("dir fail\n");
        return 1;
    }
    rogue_input_apply_direction(&st, 1, 0);
    if (!rogue_input_is_down(&st, ROGUE_KEY_RIGHT) || rogue_input_is_down(&st, ROGUE_KEY_LEFT))
    {
        printf("dir2 fail\n");
        return 1;
    }
    return 0;
}
