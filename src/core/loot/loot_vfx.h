#ifndef ROGUE_LOOT_VFX_H
#define ROGUE_LOOT_VFX_H
/* Loot visual polish effects (Phases 19.2–19.5)
   This module tracks lightweight per-item effect state independent of renderer so
   unit tests can validate timing & state transitions without graphics.
*/
#ifdef __cplusplus
extern "C" { 
#endif

#include <stdint.h>

#define ROGUE_LOOT_VFX_SPARKLE_PERIOD_MS 1200.0f
#define ROGUE_LOOT_VFX_VIEW_RADIUS 4.0f /* screen edge notifier radius */
#define ROGUE_LOOT_VFX_PULSE_WINDOW_MS 5000.0f /* last 5s before despawn triggers subtle pulse */

typedef struct RogueLootVFXState {
    float sparkle_t_ms;   /* accumulated time for sparkle cycle */
    int   beam_active;    /* high rarity vertical pillar */
    int   pulse_active;   /* within final window */
    float pulse_alpha;    /* 0..1 fade based on proximity to despawn */
} RogueLootVFXState;

void rogue_loot_vfx_reset(void);
void rogue_loot_vfx_on_spawn(int inst_index, int rarity);
void rogue_loot_vfx_on_despawn(int inst_index);
void rogue_loot_vfx_update(float dt_ms);
int  rogue_loot_vfx_get(int inst_index, RogueLootVFXState* out); /* returns 0 if invalid */
int  rogue_loot_vfx_edge_notifiers(void); /* count of items currently outside view radius */

#ifdef __cplusplus
}
#endif
#endif
