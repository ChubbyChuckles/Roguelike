#include "ui/core/ui_context.h"
#include <stdio.h>
#include <string.h>

/* Minimal harness to validate radial selector open, axis mapping and choose event. */

/* RogueUIEvent defined in header */

static int drain_kind(RogueUIContext* ctx, int kind, int* a_out)
{
    RogueUIEvent e;
    int found = 0;
    while (rogue_ui_poll_event(ctx, &e))
    {
        if (e.kind == kind)
        {
            if (a_out)
                *a_out = e.a;
            found = 1;
        }
    }
    return found;
}

int main(void)
{
    RogueUIContext ui;
    RogueUIContextConfig cfg = {0};
    cfg.max_nodes = 256;
    cfg.seed = 1234;
    cfg.arena_size = 8 * 1024;
    if (!rogue_ui_init(&ui, &cfg))
    {
        fprintf(stderr, "RADIAL_FAIL init\n");
        return 1;
    }
    rogue_ui_begin(&ui, 16.0); /* frame */
    rogue_ui_radial_open(&ui, 6);
    if (!drain_kind(&ui, ROGUE_UI_EVENT_RADIAL_OPEN, NULL))
    {
        fprintf(stderr, "RADIAL_FAIL no open event\n");
        return 2;
    }
    /* Provide controller axis pointing roughly left ( -1,0 ) which corresponds to angle pi and
     * should map near index 3 (since index 0 is up) */
    RogueUIControllerState cst = {0};
    cst.axis_x = -1.0f;
    cst.axis_y = 0.0f;
    rogue_ui_set_controller(&ui, &cst);
    rogue_ui_set_input(&ui, &(RogueUIInputState){0});
    const char* labels[6] = {"A", "B", "C", "D", "E", "F"};
    rogue_ui_radial_menu(&ui, 200, 200, 64, labels, 6);
    int sel = ui.radial.selection;
    if (sel < 0 || sel >= 6)
    {
        fprintf(stderr, "RADIAL_FAIL selection range %d\n", sel);
        return 3;
    }
    /* Simulate activate */
    RogueUIInputState in = {0};
    in.key_activate = 1;
    rogue_ui_set_input(&ui, &in);
    rogue_ui_radial_menu(&ui, 200, 200, 64, labels, 6);
    int chosen = -1;
    if (!drain_kind(&ui, ROGUE_UI_EVENT_RADIAL_CHOOSE, &chosen))
    {
        fprintf(stderr, "RADIAL_FAIL no choose event\n");
        return 4;
    }
    if (chosen != sel)
    {
        fprintf(stderr, "RADIAL_FAIL choose mismatch expected %d got %d\n", sel, chosen);
        return 5;
    }
    rogue_ui_end(&ui);
    rogue_ui_shutdown(&ui);
    printf("RADIAL_OK sel=%d\n", sel);
    return 0;
}
