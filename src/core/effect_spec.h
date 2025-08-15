/* Minimal EffectSpec system (Phase 1.2 partial) */
#ifndef ROGUE_CORE_EFFECT_SPEC_H
#define ROGUE_CORE_EFFECT_SPEC_H
#ifdef __cplusplus
extern "C" { 
#endif

/* Effect kinds (expanded later) */
typedef enum RogueEffectKind { ROGUE_EFFECT_STAT_BUFF = 0 } RogueEffectKind;

typedef struct RogueEffectSpec {
    int id;                /* registry id (assigned on register) */
    unsigned char kind;    /* RogueEffectKind */
    unsigned char target;  /* reserved for target selection (self, enemy, area) */
    unsigned short buff_type; /* maps to RogueBuffType when kind == STAT_BUFF */
    int magnitude;         /* buff magnitude (stacking additive with existing) */
    float duration_ms;     /* applied buff duration */
} RogueEffectSpec;

int  rogue_effect_register(const RogueEffectSpec* spec); /* returns id or -1 */
const RogueEffectSpec* rogue_effect_get(int id);
void rogue_effect_apply(int id, double now_ms);
void rogue_effect_reset(void); /* free registry for tests */

#ifdef __cplusplus
}
#endif
#endif
