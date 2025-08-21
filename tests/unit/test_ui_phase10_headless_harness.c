#include "ui/core/ui_context.h"
#include <assert.h>
#include <stdio.h>

static void build_frame(RogueUIContext* ctx, void* user)
{
    (void) user;
    rogue_ui_panel(ctx, (RogueUIRect){0, 0, 80, 40}, 0x11223344u);
    rogue_ui_text(ctx, (RogueUIRect){4, 4, 72, 12}, "Headless", 0xFFFFFFFFu);
}

int main()
{
    RogueUIContextConfig cfg = {.max_nodes = 8, .seed = 42u};
    uint64_t h1 = 0, h2 = 0;
    assert(rogue_ui_headless_run(&cfg, 16.6, build_frame, NULL, &h1));
    assert(rogue_ui_headless_run(&cfg, 16.6, build_frame, NULL, &h2));
    if (h1 != h2)
    {
        printf("FAIL hash mismatch %llu %llu\n", (unsigned long long) h1, (unsigned long long) h2);
        return 1;
    }
    printf("HEADLESS_OK hash=%llu\n", (unsigned long long) h1);
    return 0;
}
