#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <string.h>
#include "entities/player.h"

/* Validate encumbrance tier thresholds only (mobility scalars are integrated elsewhere). */

int main(){
    RoguePlayer p; rogue_player_init(&p);
    p.encumbrance_capacity = 100.0f;
    p.encumbrance = 10.0f; rogue_player_recalc_derived(&p); if(p.encumbrance_tier != 0){ printf("fail_tier_light=%d\n", p.encumbrance_tier); return 1; }
    p.encumbrance = 45.0f; rogue_player_recalc_derived(&p); if(p.encumbrance_tier != 1){ printf("fail_tier_medium=%d\n", p.encumbrance_tier); return 2; }
    p.encumbrance = 75.0f; rogue_player_recalc_derived(&p); if(p.encumbrance_tier != 2){ printf("fail_tier_heavy=%d\n", p.encumbrance_tier); return 3; }
    p.encumbrance = 105.0f; rogue_player_recalc_derived(&p); if(p.encumbrance_tier != 3){ printf("fail_tier_overloaded=%d\n", p.encumbrance_tier); return 4; }
    printf("phase7_encumbrance_mobility_curve: OK tiers=0,1,2,3 validated\n"); return 0; }
