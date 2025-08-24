/* Enemy attack DPS timing test: ensure average interval >= configured cooldown floor */
#define SDL_MAIN_HANDLED
#include "../../src/core/app.h"
#include "../../src/entities/enemy.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    RogueAppConfig cfg = {"DPS",         320, 180, 320, 180, 0, 0, 0, 1, ROGUE_WINDOW_WINDOWED,
                          {0, 0, 0, 255}};
    printf("dps test start\n");
    fflush(stdout);
    int ok = rogue_app_init(&cfg);
    printf("init=%d\n", ok);
    fflush(stdout);
    assert(ok);
    rogue_app_skip_start_screen();

    /* Spawn exactly one enemy at controlled distance directly adjacent so it can attack */
    RogueEnemy* e =
        rogue_test_spawn_hostile_enemy(0.3f, 0.0f); /* closer to ensure within 0.36f dist2 radius */
    assert(e);
    /* Advance one frame to absorb any immediate spawn attack */
    rogue_app_step();
    int start_health = rogue_app_player_health();
    int last_health = start_health;
    int damage_events = 0;
    int intervals[32];
    int interval_count = 0;
    int min_int = 1000000, max_int = 0;
    int ms_since_last = 0;

    /* Simulate ~10 seconds using measured dt each step (dt in seconds) */
    double sim_ms = 0.0;
    while (sim_ms < 8000.0)
    {
        rogue_app_step();
        double dt_sec = rogue_app_delta_time();
        if (dt_sec <= 0)
            dt_sec = 0.001; /* safety */
        int dt_ms = (int) (dt_sec * 1000.0 + 0.5);
        if (dt_ms < 1)
            dt_ms = 1;
        sim_ms += dt_ms;
        ms_since_last += dt_ms;
        int h = rogue_app_player_health();
        if (h < last_health)
        {
            damage_events++;
            if (ms_since_last < min_int)
                min_int = ms_since_last;
            if (ms_since_last > max_int)
                max_int = ms_since_last;
            if (interval_count < 32)
            {
                intervals[interval_count++] = ms_since_last;
            }
            ms_since_last = 0;
        }
        last_health = h; /* update baseline each frame */
        if (((int) sim_ms) % 500 == 0)
        {
            float dx = e->base.pos.x;
            float dy = e->base.pos.y;
            float dist2 = dx * dx + dy * dy;
            printf("tick ms=%.0f cooldown=%.1f dist2=%.3f hp=%d events=%d\n", sim_ms,
                   e->attack_cooldown_ms, dist2, h, damage_events);
            fflush(stdout);
        }
    }

    /* Expect at least one damage event */
    if (damage_events == 0)
    {
        printf("no enemy attacks registered: start_hp=%d end_hp=%d enemy_pos=(%.2f,%.2f) "
               "cooldown=%.1f dist2=%.3f\n",
               start_health, last_health, e->base.pos.x, e->base.pos.y, e->attack_cooldown_ms,
               (e->base.pos.x * e->base.pos.x + e->base.pos.y * e->base.pos.y));
        fflush(stdout);
        rogue_app_shutdown();
        return 1;
    }
    /* Compute average interval */
    int sum = 0;
    for (int i = 0; i < interval_count; i++)
        sum += intervals[i];
    float avg = (interval_count > 0) ? (float) sum / (float) interval_count : 0.0f;
    /* Hitstop & rendering overhead stretch intervals; ensure reasonable mid-range */
    if (avg < 1100.0f || avg > 2400.0f)
    {
        printf("avg interval out of expected range: %.2f ms (events=%d)\n", avg, damage_events);
        fflush(stdout);
        rogue_app_shutdown();
        return 1;
    }

    if (min_int < 900 || max_int > 2600)
    {
        printf("intervals out of band min=%d max=%d avg=%.1f events=%d\n", min_int, max_int, avg,
               damage_events);
        fflush(stdout);
        rogue_app_shutdown();
        return 1;
    }
    printf("DPS timing ok: events=%d avg=%.1fms min=%d max=%d\n", damage_events, avg, min_int,
           max_int);
    fflush(stdout);
    rogue_app_shutdown();
    return 0;
}
