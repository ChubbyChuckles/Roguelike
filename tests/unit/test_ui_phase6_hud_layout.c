#define SDL_MAIN_HANDLED 1
#include <stdio.h>
#include <string.h>
#include "core/hud_layout.h"

/* UI Phase 6.1 test: load HUD layout file, verify parsed coordinates propagate */
int main(){
    rogue_hud_layout_reset_defaults();
    const RogueHUDLayout* def = rogue_hud_layout();
    int def_x = def->health.x;
    /* When run via CTest, working directory is build/tests so we need ../assets */
    int ok = rogue_hud_layout_load("../assets/hud_layout.cfg");
    const RogueHUDLayout* after = rogue_hud_layout();
    int fails=0;
    if(!ok){ printf("FAIL could not load layout file\n"); fails++; }
    /* Expect health bar x unchanged (same as file) but we can still assert file values */
    if(after->health.x != 6 || after->health.y != 4 || after->health.w != 200){ printf("FAIL health bar mismatch (%d,%d,%d)\n", after->health.x,after->health.y,after->health.w); fails++; }
    if(after->mana.y <= after->health.y){ printf("FAIL mana not below health (%d vs %d)\n", after->mana.y, after->health.y); fails++; }
    /* Ensure level text positioned relative to right of health via file override (214,4) */
    if(after->level_text_x != 214 || after->level_text_y != 4){ printf("FAIL level text coords (%d,%d)\n", after->level_text_x, after->level_text_y); fails++; }
    if(!fails) printf("test_ui_phase6_hud_layout: OK (def_health_x=%d new_health_x=%d loaded=%d)\n", def_x, after->health.x, after->loaded); else printf("test_ui_phase6_hud_layout: FAIL (%d)\n", fails);
    return fails?1:0;
}
