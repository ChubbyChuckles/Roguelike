/* hud_buff_belt.h - UI Phase 6.3 Buff & Debuff icon belt */
#ifndef ROGUE_HUD_BUFF_BELT_H
#define ROGUE_HUD_BUFF_BELT_H
#include "core/buffs.h"

#define ROGUE_HUD_MAX_BUFF_ICONS 16

typedef struct RogueHUDBuffIcon { int type; int magnitude; float remaining_ms; float total_ms; } RogueHUDBuffIcon;

typedef struct RogueHUDBuffBeltState {
    RogueHUDBuffIcon icons[ROGUE_HUD_MAX_BUFF_ICONS];
    int count;
} RogueHUDBuffBeltState;

void rogue_hud_buff_belt_refresh(RogueHUDBuffBeltState* st, double now_ms);
void rogue_hud_buff_belt_render(const RogueHUDBuffBeltState* st, int screen_w);

#endif
