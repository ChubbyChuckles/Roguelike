/* test_enemy_difficulty_phase5.c - AI Behavior Intensity Layers tests
 * Validates escalation & de-escalation thresholds and hysteresis.
 */
#define SDL_MAIN_HANDLED 1
#include "core/app_state.h"
#include "core/enemy/enemy_ai_intensity.h"
#include "entities/enemy.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Minimal stub enemy for logic test (no world/app dependency for score math) */
static RogueEnemy make_enemy(void)
{
    RogueEnemy e;
    memset(&e, 0, sizeof e);
    e.ai_intensity = 1;
    e.ai_intensity_score = 1.0f;
    return e;
}

int main(void)
{
    RogueEnemy e = make_enemy();
    /* Simulate proximity + player low health escalating */
    for (int i = 0; i < 400; i++)
    { /* 400ms ticks * 10 iterations ~4s */
        /* Fake player low health & pack deaths */
        rogue_ai_intensity_update(&e, 400.0f, 1, (i < 5) ? 1 : 0);
    }
    if (e.ai_intensity < 2)
    {
        printf("FAIL escalation did not reach aggressive tier (tier=%d score=%.2f)\n",
               e.ai_intensity, e.ai_intensity_score);
        return 1;
    }
    else
    {
        printf("stage1 ok tier=%d score=%.2f\n", e.ai_intensity, e.ai_intensity_score);
    }
    /* Force frenzied by continuing triggers */
    for (int i = 0; i < 800; i++)
    {
        rogue_ai_intensity_update(&e, 300.0f, 1, 0);
    }
    if (e.ai_intensity < 3)
    {
        printf("FAIL escalation did not reach frenzied tier (tier=%d score=%.2f)\n", e.ai_intensity,
               e.ai_intensity_score);
        return 1;
    }
    else
    {
        printf("stage2 ok frenzied tier=%d score=%.2f\n", e.ai_intensity, e.ai_intensity_score);
    }
    /* Now simulate calm high player health & distance to de-escalate */
    /* Place enemy far to trigger calm decay (simulate distance); update global player health for
     * high health path */
    extern struct RogueAppState g_app; /* declare to set player state (test context) */
    g_app.player.max_health = 100;
    g_app.player.health = 100;
    e.base.pos.x = 0;
    e.base.pos.y = 0;
    g_app.player.base.pos.x = 100.0f;
    g_app.player.base.pos.y = 100.0f; /* large distance */
    for (int i = 0; i < 6000; i++)
    {
        rogue_ai_intensity_update(&e, 10.0f, 0, 0);
    }
    printf("after calm tier=%d score=%.2f\n", e.ai_intensity, e.ai_intensity_score);
    if (e.ai_intensity > 2)
    {
        printf("FAIL de-escalation did not drop below frenzied (tier=%d score=%.2f)\n",
               e.ai_intensity, e.ai_intensity_score);
        return 1;
    }
    printf("OK test_enemy_difficulty_phase5\n");
    return 0;
}
