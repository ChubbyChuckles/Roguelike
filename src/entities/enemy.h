#ifndef ROGUE_ENTITIES_ENEMY_H
#define ROGUE_ENTITIES_ENEMY_H
#include "entities/entity.h"
#include "graphics/sprite.h"

typedef enum RogueEnemyAIState { ROGUE_ENEMY_AI_PATROL=0, ROGUE_ENEMY_AI_AGGRO, ROGUE_ENEMY_AI_DEAD } RogueEnemyAIState;

typedef struct RogueEnemyTypeDef {
    char name[32];
    int weight;          /* spawn weighting (currently unused basic single type) */
    int group_min;
    int group_max;
    int patrol_radius;   /* tiles */
    int aggro_radius;    /* tiles */
    float speed;         /* tiles per second */
    int pop_target;      /* desired population for this type */
    int xp_reward;       /* xp per kill */
    float loot_chance;   /* 0..1 */
    /* Animation sheets (side-only) */
    RogueTexture idle_tex, run_tex, death_tex;
    RogueSprite  idle_frames[8]; int idle_count;
    RogueSprite  run_frames[8];  int run_count;
    RogueSprite  death_frames[8];int death_count;
} RogueEnemyTypeDef;

typedef struct RogueEnemy {
    RogueEntity base;
    unsigned char team_id; /* Phase 5.4: faction/team (e.g., 1=enemies) */
    int type_index;      /* index into type def array */
    int health;
    int max_health;      /* persistent max for ratio */
    int alive;           /* 1 alive, 0 removed */
    float hurt_timer;    /* ms */
    float anim_time;     /* ms */
    int anim_frame;
    RogueEnemyAIState ai_state;
    float anchor_x, anchor_y; /* group anchor */
    float patrol_target_x, patrol_target_y; /* current patrol target */
    int facing;          /* 1=left,2=right for rendering flip */
    /* Smooth tint (stored as 0-255 range floats for modulation) */
    float tint_r, tint_g, tint_b;
    /* Death fade alpha (1 -> 0) */
    float death_fade;
    float tint_phase;   /* ms accumulator for pulsing */
    float flash_timer;  /* brief hit flash (ms) */
    float attack_cooldown_ms; /* time until next attack allowed */
    int crit_chance;    /* percent flat */
    int crit_damage;    /* percent bonus over 100 */
    /* --- Status buildup placeholders (Phase 1A scaffold) --- */
    float bleed_buildup; /* accumulates until threshold triggers bleed (future) */
    float frost_buildup; /* accumulates; future slow/freeze effects */
    /* --- Phase 2: mitigation stats (simple baseline) --- */
    int armor;           /* reduces physical (flat first then percent) */
    int resist_physical; /* percent 0-90 (separate from armor) */
    int resist_fire;     /* percent 0-90 */
    int resist_frost;    /* percent 0-90 */
    int resist_arcane;   /* percent 0-90 */
    int resist_bleed;    /* percent (future DOT mitigation) */
    int resist_poison;   /* percent */
    /* --- Phase 3.1: Add guard & poise meters for future stagger / block systems --- */
    float guard_meter; float guard_meter_max; /* enemies may use in later AI phases */
    float poise; float poise_max;             /* depleted -> stagger (future Phase 4 reactions) */
    int staggered; float stagger_timer_ms;    /* Phase 3.3: simple stagger flag+timer (placeholder reaction) */
} RogueEnemy;

#define ROGUE_MAX_ENEMIES 256
#define ROGUE_MAX_ENEMY_TYPES 16

int rogue_enemy_load_config(const char* path, RogueEnemyTypeDef types[], int* inout_type_count);

#endif
