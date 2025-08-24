/* Central damage & cooldown calculation utilities */
#ifndef ROGUE_CORE_DAMAGE_CALC_H
#define ROGUE_CORE_DAMAGE_CALC_H
#include "skills/skills.h"

/* Synergy buckets enumeration (extend as needed) */
enum
{
    ROGUE_SYNERGY_FIRE_POWER = 0,
    ROGUE_SYNERGY_COUNT
};

/* Compute fireball damage using skill rank + fire synergy */
int rogue_damage_fireball(int fireball_skill_id);

/* Compute fireball cooldown based on definition & rank (centralized) */
float rogue_cooldown_fireball_ms(int fireball_skill_id);

#endif
