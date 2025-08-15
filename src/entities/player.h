/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef ROGUE_ENTITIES_PLAYER_H
#define ROGUE_ENTITIES_PLAYER_H

#include "entities/entity.h"

typedef struct RoguePlayer
{
    RogueEntity base;
    int health;
    int max_health; /* derived */
    int mana;
    int max_mana; /* derived */
    /* Action Points (Phase 1.5 core) */
    int action_points;      /* current AP pool */
    int max_action_points;  /* cap */
    int facing; /* 0=down,1=left,2=right,3=up */
    float anim_time;
    int anim_frame;
    int level;
    int xp;
    int xp_to_next;
    /* Core stats */
    int strength;
    int dexterity;
    int vitality;
    int intelligence;
    int crit_chance;   /* percent (0-100) additional flat crit chance */
    int crit_damage;   /* percent bonus over 100; e.g. 50 => 1.5x */
    /* --- Phase 2 Mitigation / Penetration Stats (baseline) --- */
    int armor;         /* flat physical mitigation base */
    int resist_physical; /* percent 0-90 physical (percent) */
    int resist_fire;   /* percent 0-90 */
    int resist_frost;  /* percent 0-90 */
    int resist_arcane; /* percent 0-90 */
    int resist_bleed;  /* placeholder */
    int resist_poison; /* placeholder */
    int pen_flat;      /* flat armor penetration */
    int pen_percent;   /* percent armor penetration (0-100) */
} RoguePlayer;

void rogue_player_init(RoguePlayer* p);
void rogue_player_recalc_derived(RoguePlayer* p);
#endif
