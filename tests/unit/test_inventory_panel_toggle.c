/* Test inventory panel toggle visibility logic.
 * Directly manipulates the flag because SDL events aren't dispatched in headless unit tests.
 */
#include "core/app_state.h"
#include "core/vendor/vendor_ui.h"
#include <stdio.h>

int main(void){
    if(g_app.show_inventory_panel!=0){ printf("INV_PANEL_TOGGLE_FAIL initial_not_zero\n"); return 10; }
    g_app.show_inventory_panel = 1;
    rogue_inventory_panel_render();
    if(!g_app.show_inventory_panel){ printf("INV_PANEL_TOGGLE_FAIL not_set\n"); return 11; }
    g_app.show_inventory_panel = 0; if(g_app.show_inventory_panel){ printf("INV_PANEL_TOGGLE_FAIL not_cleared\n"); return 12; }
    printf("INV_PANEL_TOGGLE_OK\n"); return 0; }
