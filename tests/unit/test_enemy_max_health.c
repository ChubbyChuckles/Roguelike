#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED
#endif
#include "../../src/entities/enemy.h"
#include <stdio.h>

int main(void)
{
    RogueEnemy e;
    e.max_health = 15;
    e.health = 10;
    if (e.max_health < e.health)
    {
        printf("max health tracking fail\n");
        return 1;
    }
    float ratio = (e.max_health > 0) ? (float) e.health / (float) e.max_health : 0.0f;
    if (ratio <= 0.0f || ratio >= 1.0f)
    {
        printf("ratio unexpected\n");
        return 1;
    }
    return 0;
}
