#include "core/vendor/economy.h"
#include "core/vendor/vendor.h"
#include "core/inventory.h"
#include "core/app_state.h"
#include <stdio.h>

/* Headless test: Simulate opening vendor, selecting item, opening confirmation, insufficient funds flash, then adding gold and confirming purchase */
int main(void){
    /* Fabricate vendor item (no item defs needed for logic of confirmation state) */
    RogueVendorItem v; v.def_index = 1; v.rarity=2; v.price=500; /* fixed price */
    const RogueVendorItem* vi=&v;
    g_app.show_vendor_panel=1; g_app.vendor_selection=0;
    rogue_econ_reset(); /* zero gold */
    int price = v.price; /* direct */
    /* Simulate pressing return twice: first opens modal, second attempts buy (insufficient) -> sets flash timer */
    if(g_app.vendor_confirm_active){ fprintf(stderr,"VT_FAIL unexpected active start\n"); return 14; }
    /* Open modal */
    { /* mimic first RETURN branch of input handler */
        if(!g_app.vendor_confirm_active){ g_app.vendor_confirm_active=1; g_app.vendor_confirm_def_index=vi->def_index; g_app.vendor_confirm_price=price; g_app.vendor_insufficient_flash_ms=0.0; }
    }
    if(!g_app.vendor_confirm_active){ fprintf(stderr,"VT_FAIL modal not active\n"); return 15; }
    /* Attempt accept with insufficient funds */
    if(rogue_econ_gold() >= g_app.vendor_confirm_price){ fprintf(stderr,"VT_FAIL gold unexpectedly sufficient\n"); return 16; }
    { if(rogue_econ_gold() >= g_app.vendor_confirm_price){ } else { g_app.vendor_insufficient_flash_ms = 480.0; } }
    if(g_app.vendor_insufficient_flash_ms <= 0){ fprintf(stderr,"VT_FAIL no flash set\n"); return 17; }
    /* Add gold and accept again */
    rogue_econ_add_gold(price);
    if(rogue_econ_gold() < g_app.vendor_confirm_price){ fprintf(stderr,"VT_FAIL still insufficient after add\n"); return 18; }
    int inv_before = rogue_inventory_get_count(vi->def_index);
    { if(rogue_econ_gold() >= g_app.vendor_confirm_price){ /* simulate purchase success */ rogue_inventory_add(vi->def_index,1); g_app.vendor_confirm_active=0; } }
    if(g_app.vendor_confirm_active){ fprintf(stderr,"VT_FAIL modal not closed\n"); return 19; }
    int inv_after = rogue_inventory_get_count(vi->def_index);
    if(inv_after != inv_before+1){ fprintf(stderr,"VT_FAIL inv delta %d->%d (def=%d)\n", inv_before, inv_after, vi->def_index); return 20; }
    printf("VT_OK price=%d flash_ms=%.1f\n", price, g_app.vendor_insufficient_flash_ms);
    return 0;
}
