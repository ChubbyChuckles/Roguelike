#define SDL_MAIN_HANDLED 1
#include "ui/core/ui_context.h"
#include "ui/core/ui_theme.h"
#include <stdio.h>
#include <string.h>

int main()
{
    RogueUIContext ctx;
    RogueUIContextConfig cfg = {256, 1234, 32 * 1024};
    if (!rogue_ui_init(&ctx, &cfg))
    {
        printf("FAIL init\n");
        return 1;
    }
    rogue_ui_begin(&ctx, 16.0); /* frame start */
    /* Build simple UI with interactive widgets */
    RogueUIRect r = {10, 10, 300, 200};
    int row = rogue_ui_row_begin(&ctx, r, 4, 4);
    RogueUIRect b1;
    rogue_ui_row_next(&ctx, row, 80, 24, &b1);
    rogue_ui_button(&ctx, b1, "Play", 0x303030FF, 0xFFFFFFFF);
    RogueUIRect b2;
    rogue_ui_row_next(&ctx, row, 80, 24, &b2);
    int tog = 1;
    rogue_ui_toggle(&ctx, b2, "Music", &tog, 0x202020FF, 0x404080FF, 0xFFFFFFFF);
    RogueUIRect b3;
    rogue_ui_row_next(&ctx, row, 100, 24, &b3);
    float val = 0.5f;
    rogue_ui_slider(&ctx, b3, 0.0f, 1.0f, &val, 0x101010FF, 0x8080FFFF);
    char textbuf[32] = {0};
    RogueUIRect t1 = {10, 50, 120, 20};
    rogue_ui_text_input(&ctx, t1, textbuf, 32, 0x202020FF, 0xFFFFFFFF);
    /* Focus audit overlays */
    rogue_ui_focus_audit_enable(&ctx, 1);
    int overlays = rogue_ui_focus_audit_emit_overlays(&ctx, 0xFF00FFFF);
    if (overlays < 4)
    {
        printf("FAIL overlays %d\n", overlays);
        return 1;
    }
    /* Export focus order */
    char order[256];
    size_t olen = rogue_ui_focus_order_export(&ctx, order, sizeof order);
    if (olen == 0 || strstr(order, "Play") == NULL)
    {
        printf("FAIL order export\n");
        return 1;
    }
    /* Narration */
    rogue_ui_narrate(&ctx, "Button Play focused");
    if (strcmp(rogue_ui_last_narration(&ctx), "Button Play focused") != 0)
    {
        printf("FAIL narration store\n");
        return 1;
    }
    /* Reduced motion flag */
    rogue_ui_set_reduced_motion(&ctx, 1);
    if (!rogue_ui_reduced_motion(&ctx))
    {
        printf("FAIL reduced motion flag\n");
        return 1;
    }
    rogue_ui_end(&ctx);
    rogue_ui_shutdown(&ctx);
    printf("test_ui_phase7_accessibility: OK\n");
    return 0;
}
