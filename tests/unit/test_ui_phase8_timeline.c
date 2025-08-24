#define SDL_MAIN_HANDLED 1
#include "../../src/ui/core/ui_animation.h"
#include "../../src/ui/core/ui_context.h"
#include <stdio.h>

static uint32_t id_for(const char* s) { return rogue_ui_make_id(s); }

int main()
{
    RogueUIContext ctx;
    RogueUIContextConfig cfg = {128, 1234, 16 * 1024};
    if (!rogue_ui_init(&ctx, &cfg))
    {
        printf("FAIL init\n");
        return 1;
    }
    rogue_ui_begin(&ctx, 16.0);
    RogueUIRect r = {10, 10, 80, 24};
    int b = rogue_ui_button(&ctx, r, "TLBtn", 0x101010FF, 0xFFFFFFFF);
    if (b < 0)
    {
        printf("FAIL button create\n");
        return 1;
    }
    uint32_t id = ctx.nodes[b].id_hash;
    RogueUITimelineKeyframe kf[3] = {{0.0f, 0.5f, 0.0f, ROGUE_EASE_CUBIC_OUT},
                                     {0.5f, 1.2f, 1.0f, ROGUE_EASE_CUBIC_IN_OUT},
                                     {1.0f, 1.0f, 0.8f, ROGUE_EASE_CUBIC_IN}};
    rogue_ui_timeline_play(&ctx, id, kf, 3, 600.0f, ROGUE_UI_TIMELINE_REPLACE);
    rogue_ui_end(&ctx);
    float s0 = rogue_ui_timeline_scale(&ctx, id, NULL);
    float a0 = rogue_ui_timeline_alpha(&ctx, id, NULL);
    if (s0 < 0.49f || a0 > 0.05f)
    {
        printf("FAIL initial sample scale=%f alpha=%f\n", s0, a0);
        return 1;
    }
    /* initial sample ok */
    /* Advance ~ half duration */
    for (int i = 0; i < 20; i++)
    {
        rogue_ui_begin(&ctx, 16.0);
        rogue_ui_end(&ctx);
    }
    int active_mid = 0;
    float sm = rogue_ui_timeline_scale(&ctx, id, &active_mid);
    float am = rogue_ui_timeline_alpha(&ctx, id, NULL);
    if (!active_mid)
    {
        printf("FAIL timeline inactive mid (sm=%f am=%f t_mid_frames=20)\n", sm, am);
        return 1;
    }
    /* mid sample */
    if (sm < 0.9f || sm > 1.25f)
    {
        printf("FAIL scale mid %f\n", sm);
        return 1;
    }
    if (am < 0.6f || am > 1.05f)
    {
        printf("FAIL alpha mid %f\n", am);
        return 1;
    }
    /* Finish */
    for (int i = 0; i < 30; i++)
    {
        rogue_ui_begin(&ctx, 16.0);
        rogue_ui_end(&ctx);
    }
    int active_end = 0;
    float se = rogue_ui_timeline_scale(&ctx, id, &active_end);
    float ae = rogue_ui_timeline_alpha(&ctx, id, NULL);
    if (active_end)
    {
        printf("FAIL timeline still active end (se=%f ae=%f)\n", se, ae);
        return 1;
    }
    /* end sample */
    if (se < 0.95f || se > 1.05f)
    {
        printf("FAIL end scale %f\n", se);
        return 1;
    }
    if (ae < 0.75f || ae > 1.05f)
    {
        printf("FAIL end alpha %f\n", ae);
        return 1;
    }
    printf("test_ui_phase8_timeline: OK\n");
    rogue_ui_shutdown(&ctx);
    printf("test_ui_phase8_timeline: OK\n");
    return 0;
}
