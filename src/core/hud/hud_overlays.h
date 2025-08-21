/* Phase 6.5/6.6/6.7 HUD overlays: minimap toggle (existing minimap.c), alerts, metrics */
#ifndef ROGUE_CORE_HUD_OVERLAYS_H
#define ROGUE_CORE_HUD_OVERLAYS_H
#include "core/app_state.h"
/* Initialize alert system state */
void rogue_alerts_reset(void);
/* Raise specific alerts */
void rogue_alert_level_up(void);
void rogue_alert_low_health(void);
void rogue_alert_vendor_restock(void);
/* Per-frame update & render (called from hud.c) */
void rogue_alerts_update_and_render(float dt_ms);
/* Metrics overlay render (if enabled) */
void rogue_metrics_overlay_render(void);
#endif /* ROGUE_CORE_HUD_OVERLAYS_H */
