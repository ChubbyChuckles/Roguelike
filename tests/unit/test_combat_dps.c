/* Enemy attack DPS timing test: ensure average interval >= configured cooldown floor */
#define SDL_MAIN_HANDLED
#include <assert.h>
#include <stdio.h>
#include "core/app.h"

int main(){
    RogueAppConfig cfg = {"DPS", 320,180,320,180,0,0,0,1,ROGUE_WINDOW_WINDOWED,{0,0,0,255}};
    assert(rogue_app_init(&cfg));
    rogue_app_skip_start_screen();

    /* Spawn exactly one enemy at controlled distance directly adjacent so it can attack */
    RogueEnemy* e = rogue_test_spawn_hostile_enemy(0.5f, 0.0f);
    assert(e);
    int start_health = rogue_app_player_health();
    int last_health = start_health;
    int damage_events = 0;
    int intervals[16];
    int interval_count = 0;
    int ms_since_last = 0;

    /* Simulate 10 seconds (10000 frames at 1ms per step) */
    for(int frame=0; frame<10000; ++frame){
        rogue_app_step();
        ms_since_last++;
        int h = rogue_app_player_health();
        if(h < last_health){
            damage_events++;
            if(interval_count < 16){ intervals[interval_count++] = ms_since_last; }
            ms_since_last = 0;
            last_health = h;
        }
    }

    /* Expect at least one damage event */
    assert(damage_events > 0);
    /* Compute average interval */
    int sum=0; for(int i=0;i<interval_count;i++) sum += intervals[i];
    float avg = (interval_count>0) ? (float)sum / (float)interval_count : 0.0f;
    /* Current enemy cooldown random range approx 650-1050ms, allow some jitter. Require avg >= 600ms */
    if(avg < 600.0f){
        printf("avg interval too low: %.2f ms (events=%d)\n", avg, damage_events);
        rogue_app_shutdown();
        return 1;
    }

    printf("DPS timing ok: events=%d avg=%.1fms\n", damage_events, avg);
    rogue_app_shutdown();
    return 0;
}
