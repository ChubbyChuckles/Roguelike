#ifndef ROGUE_START_SCREEN_H
#define ROGUE_START_SCREEN_H
#include "core/app_state.h"

/* Phase 1.1: Start screen states for simple transitions */
typedef enum RogueStartScreenState
{
    ROGUE_START_FADE_IN = 0,
    ROGUE_START_MENU,
    ROGUE_START_FADE_OUT
} RogueStartScreenState;

/* Phase 2.3: Background image scaling modes */
typedef enum RogueStartBGScale
{
    ROGUE_BG_COVER = 0,
    ROGUE_BG_CONTAIN = 1,
    ROGUE_BG_AUTO = 2 /* reserved: choose based on aspect or config */
} RogueStartBGScale;

void rogue_start_screen_update_and_render(void);
int rogue_start_screen_active(void);
/* Optional: change background scaling mode at runtime (tests/tools). */
void rogue_start_screen_set_bg_scale(RogueStartBGScale mode);

/* Phase 3.3 test hooks: expose current menu label and tooltip. */
const char* rogue_start_menu_label(int index);
const char* rogue_start_tooltip_text(void);

#endif
