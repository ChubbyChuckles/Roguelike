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
    unsigned char team_id; /* Phase 5.4: team/faction for friendly-fire filtering (0=player faction) */
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
    /* --- Phase 3.1: Separate resource meters (stamina vs guard vs poise) --- */
    float guard_meter;      /* depletes while guarding (directional block) */
    float guard_meter_max;
    float poise;            /* temporary stability buffer; damage to poise causes stagger when depleted */
    float poise_max;
    /* --- Phase 3.2: Encumbrance rating & derived tier (affects stamina regen & move speed) --- */
    float encumbrance;       /* current total equipped weight */
    float encumbrance_capacity; /* capacity before overload */
    int encumbrance_tier;    /* 0=light,1=medium,2=heavy,3=overloaded */
    /* --- Phase 3.8+ Guard / Parry additions --- */
    int guarding;            /* 1 if currently holding guard */
    float guard_active_time_ms; /* time spent in current guard state */
    float perfect_guard_window_ms; /* cached window threshold (configurable) */
    float poise_regen_delay_ms; /* delay before poise begins regenerating after damage */
    /* --- Phase 4.1+ Hit Reactions & I-Frames --- */
    int reaction_type;          /* 0=none,1=light_flinch,2=stagger,3=knockdown,4=launch */
    float reaction_timer_ms;    /* remaining reaction lock time */
    float iframes_ms;           /* remaining invulnerability frames (dodge/parry) */
    /* --- Phase 4.4 Crowd Control (stun, root, slow, disarm placeholders) --- */
    float cc_stun_ms;           /* cannot move or attack */
    float cc_root_ms;           /* cannot move but can attack */
    float cc_slow_ms;           /* slow effect duration */
    float cc_slow_pct;          /* 0..1 fraction speed reduction while slow active */
    float cc_disarm_ms;         /* cannot attack (still move) */
    /* --- Phase 4.5 Reaction Cancel Windows & Directional Influence (DI) --- */
    float reaction_total_ms;    /* original full duration of current reaction for fraction calculations */
    int reaction_canceled_early;/* 1 if player performed early cancel inside window */
    float reaction_di_accum_x;  /* accumulated DI offset (x) for current reaction */
    float reaction_di_accum_y;  /* accumulated DI offset (y) for current reaction */
    float reaction_di_max;      /* per‑reaction cap radius (set on apply) */
    /* --- Phase 5.6 Lock-On Subsystem --- */
    unsigned char lock_on_active; /* 1 if currently locked */
    int lock_on_target_index;     /* enemy index targeted */
    float lock_on_radius;         /* acquisition radius (tiles) */
    float lock_on_switch_cooldown_ms; /* prevents rapid cycling */
    /* --- Phase 6.5 Riposte Window --- */
    float riposte_ms;            /* time window after successful parry/perfect guard to perform riposte */
    /* --- Phase 7 Weapons & Stances --- */
    int equipped_weapon_id;      /* -1 none */
    int combat_stance;           /* 0=balanced,1=aggressive,2=defensive */
    /* --- Phase 7.4 Weapon Infusions --- */
    int weapon_infusion;         /* 0=none,1=fire,2=frost,3=arcane,4=bleed,5=poison */
} RoguePlayer;

void rogue_player_init(RoguePlayer* p);
void rogue_player_recalc_derived(RoguePlayer* p);
#endif
