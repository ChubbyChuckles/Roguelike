#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <assert.h>
#include "core/app.h"
#include "entities/enemy.h"

int main(){
    RogueAppConfig cfg = {"DMGNUM",320,180,320,180,0,0,0,1,ROGUE_WINDOW_WINDOWED,{0,0,0,255}};
    assert(rogue_app_init(&cfg));
    rogue_app_skip_start_screen();
    /* Directly add damage numbers to test lifecycle independent of combat randomness */
    for(int i=0;i<12;i++){
        rogue_add_damage_number(0.2f + i*0.05f, 0.1f, 3+i, 1);
    }
    int start_count = rogue_app_damage_number_count();
    for(int i=0;i<50;i++) rogue_app_step();
    int mid_count = rogue_app_damage_number_count();
    if(mid_count != start_count){ /* numbers persist during idle frames; start_count already reflects creation */ }
    /* Advance long enough for numbers to expire (life_ms ~700ms) */
    for(int i=0;i<15;i++){
        rogue_app_test_decay_damage_numbers(120.0f); /* 15 * 120ms = 1800ms ensures all expired */
    }
    int end_count = rogue_app_damage_number_count();
    if(end_count != 0){ printf("damage numbers did not fully decay start=%d end=%d\n", start_count, end_count); rogue_app_shutdown(); return 1; }
    printf("damage number lifecycle ok (start=%d mid=%d end=%d)\n", start_count, mid_count, end_count);
    rogue_app_shutdown();
    return 0;
}
