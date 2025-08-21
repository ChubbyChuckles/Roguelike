/* Phase 12.1-12.4 minimal UI helpers test: panel build, layered tooltip determinism, proc preview
 * non-negative */
#define SDL_MAIN_HANDLED 1
#include "core/equipment/equipment_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Provide minimal stubs for dependencies so the tooltip functions can operate without full engine
 * context. */
int rogue_minimap_ping_loot(float x, float y, int r)
{
    (void) x;
    (void) y;
    (void) r;
    return 0;
}

int main(void)
{
    char buf[512];
    char* r = rogue_equipment_panel_build(buf, sizeof buf);
    assert(r);
    assert(strstr(buf, "[Weapons]"));
    unsigned int h1 = rogue_item_tooltip_hash(-1, -1);
    unsigned int h2 = rogue_item_tooltip_hash(-1, -1);
    assert(h1 == h2);
    float pv = rogue_equipment_proc_preview_dps();
    assert(pv >= 0.f);
    printf("equipment_phase12_ui_ok\n");
    return 0;
}
