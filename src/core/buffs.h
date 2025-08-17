/* Timed buff system */
#ifndef ROGUE_CORE_BUFFS_H
#define ROGUE_CORE_BUFFS_H
#ifdef __cplusplus
extern "C" { 
#endif

typedef enum RogueBuffType { ROGUE_BUFF_POWER_STRIKE = 0, ROGUE_BUFF_STAT_STRENGTH = 1, ROGUE_BUFF_MAX } RogueBuffType;

typedef struct RogueBuff { int active; RogueBuffType type; double end_ms; int magnitude; } RogueBuff;

void rogue_buffs_init(void);
void rogue_buffs_update(double now_ms);
int  rogue_buffs_apply(RogueBuffType type, int magnitude, double duration_ms, double now_ms);
int  rogue_buffs_get_total(RogueBuffType type);
/* New Phase 7.3 helpers for persistence (non-mutating):
	rogue_buffs_active_count returns number of active buff records.
	rogue_buffs_get_active copies the i-th active buff (0..count-1) into *out and returns 1 on success. */
int  rogue_buffs_active_count(void);
int  rogue_buffs_get_active(int index, RogueBuff* out);
/* Phase 6.3 snapshot: copy up to max active buffs into out array, returns count. now_ms used to prune expired. */
int  rogue_buffs_snapshot(RogueBuff* out, int max, double now_ms);

#ifdef __cplusplus
}
#endif
#endif
