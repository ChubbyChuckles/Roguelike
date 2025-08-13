#ifndef ROGUE_ENTITIES_ENEMY_H
#define ROGUE_ENTITIES_ENEMY_H
#include "entities/entity.h"

typedef enum RogueEnemyType { ROGUE_ENEMY_SLIME=0 } RogueEnemyType;

typedef struct RogueEnemy {
    RogueEntity base;
    RogueEnemyType type;
    int health;
    int alive; /* 1 alive,0 dead */
    float hurt_timer; /* brief flash / invuln */
} RogueEnemy;

#define ROGUE_MAX_ENEMIES 128

#endif
