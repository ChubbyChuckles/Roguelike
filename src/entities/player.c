#include "entities/player.h"

void rogue_player_init(RoguePlayer* p)
{
    p->base.pos.x = 0.0f;
    p->base.pos.y = 0.0f;
    p->base.vel.x = 0.0f;
    p->base.vel.y = 0.0f;
    p->health = 3;
}
