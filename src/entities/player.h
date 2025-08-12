#ifndef ROGUE_ENTITIES_PLAYER_H
#define ROGUE_ENTITIES_PLAYER_H

#include "entities/entity.h"

typedef struct RoguePlayer {
    RogueEntity base;
    int health;
} RoguePlayer;

void rogue_player_init(RoguePlayer *p);

#endif
