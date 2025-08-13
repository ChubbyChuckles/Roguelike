/* Timed buff system */
#ifndef ROGUE_CORE_BUFFS_H
#define ROGUE_CORE_BUFFS_H
#ifdef __cplusplus
extern "C" { 
#endif

typedef enum RogueBuffType { ROGUE_BUFF_POWER_STRIKE = 0, ROGUE_BUFF_MAX } RogueBuffType;

typedef struct RogueBuff { int active; RogueBuffType type; double end_ms; int magnitude; } RogueBuff;

void rogue_buffs_init(void);
void rogue_buffs_update(double now_ms);
int  rogue_buffs_apply(RogueBuffType type, int magnitude, double duration_ms, double now_ms);
int  rogue_buffs_get_total(RogueBuffType type);

#ifdef __cplusplus
}
#endif
#endif
