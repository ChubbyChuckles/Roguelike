#include "entities/player.h"
#include <stdio.h>

int main(void)
{
    RoguePlayer p;
    rogue_player_init(&p);
    int expected = 300 + p.vitality*2 + (p.level-1)*15;
    if (p.health != expected)
    {
        printf("player health init fail\n");
        return 1;
    }
    if (p.base.pos.x != 0.0f || p.base.pos.y != 0.0f)
    {
        printf("player pos init fail\n");
        return 1;
    }
    return 0;
}
