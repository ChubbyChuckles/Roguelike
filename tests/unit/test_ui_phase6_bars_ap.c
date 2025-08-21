#define SDL_MAIN_HANDLED 1
#include <stdio.h>
#include "core/hud/hud_bars.h"

/* UI Phase 6.2 test: layered smoothing + AP bar fractions */
int main(){
    RogueHUDBarsState st={0};
    // Initialize with full pools (implicit)
    rogue_hud_bars_update(&st, 100,100, 80,80, 60,60, 16);
    if(rogue_hud_health_primary(&st) != 1.0f || rogue_hud_ap_primary(&st) != 1.0f){ printf("FAIL init full fractions\n"); return 1; }
    // Apply damage: drop health to 50, mana to 40, AP to 30
    rogue_hud_bars_update(&st, 50,100, 40,80, 30,60, 16); // one frame
    float hp_p = rogue_hud_health_primary(&st); float hp_s = rogue_hud_health_secondary(&st);
    if(hp_p < 0.49f || hp_p > 0.51f){ printf("FAIL hp primary %f\n", hp_p); return 1; }
    if(hp_s <= hp_p){ printf("FAIL hp secondary not lagging (%f vs %f)\n", hp_s, hp_p); return 1; }
    // Advance simulated time enough for secondary to catch up
    for(int i=0;i<120;i++){ rogue_hud_bars_update(&st, 50,100, 40,80, 30,60, 16); }
    hp_s = rogue_hud_health_secondary(&st); if(hp_s < 0.49f || hp_s > 0.51f){ printf("FAIL hp secondary did not settle (%f)\n", hp_s); return 1; }
    // Heal back to full: secondary should snap instantly
    rogue_hud_bars_update(&st, 100,100, 80,80, 60,60, 16);
    if(rogue_hud_health_secondary(&st) != 1.0f){ printf("FAIL secondary did not snap on heal (%f)\n", rogue_hud_health_secondary(&st)); return 1; }
    // AP fraction check (should be full)
    if(rogue_hud_ap_primary(&st) != 1.0f){ printf("FAIL ap primary heal snap\n"); return 1; }
    printf("test_ui_phase6_bars_ap: OK\n");
    return 0;
}
