#include "ui/crafting_ui.h"
#include "core/crafting.h"
#include "core/inventory.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Minimal harness: seed recipe registry with a few mock recipes if empty. We assume test environment provides some. */
static int ensure_min_recipes(void){ int n=rogue_craft_recipe_count(); return n; }

int main(void){
    ensure_min_recipes();
    /* store baseline render count */
    RogueUIContext ctx={0};
    rogue_crafting_ui_set_search("");
    int all = rogue_crafting_ui_render_panel(&ctx,0,0,300,200);
    /* set a search unlikely to match anything */
    rogue_crafting_ui_set_search("__nope__");
    int none = rogue_crafting_ui_render_panel(&ctx,0,0,300,200);
    if(none!=0 && none>=all){ fprintf(stderr,"P8_FAIL search_filter none=%d all=%d\n", none, all); return 10; }
    rogue_crafting_ui_set_search("");
    /* text only toggle changes mode but still returns same or fewer lines */
    rogue_crafting_ui_set_text_only(1);
    /* test batch quantity affects panel output but still renders */
    rogue_crafting_ui_set_batch(3);
    int txt = rogue_crafting_ui_render_panel(&ctx,0,0,300,200);
    rogue_crafting_ui_set_text_only(0);
    if(txt<=0){ fprintf(stderr,"P8_FAIL text_only_count %d\n", txt); return 11; }
    if(rogue_crafting_ui_get_batch()!=3){ fprintf(stderr,"P8_FAIL batch_state %d\n", rogue_crafting_ui_get_batch()); return 13; }
    /* Queue progress (empty expected 0) */
    int q = rogue_crafting_ui_render_queue(&ctx,0,0,300,60);
    if(q!=0){ fprintf(stderr,"P8_FAIL queue_nonzero %d\n", q); return 14; }
    /* Gathering overlay (no nodes expected 0) */
    int g = rogue_crafting_ui_render_gather_overlay(&ctx,0,0,300,60);
    if(g!=0){ fprintf(stderr,"P8_FAIL gather_overlay_nonzero %d\n", g); return 15; }
    float risk = rogue_crafting_ui_expected_fracture_damage(10);
    if(risk < 0.1f || risk > 5.0f){ fprintf(stderr,"P8_FAIL risk_range %.2f\n", risk); return 12; }
    rogue_crafting_ui_render_enhancement_risk(&ctx,0,0,10);
    rogue_crafting_ui_render_material_ledger(&ctx,0,0,400,200);
    printf("CRAFT_P8_OK all=%d txt=%d risk=%.2f batch=%d\n", all, txt, risk, rogue_crafting_ui_get_batch());
    return 0;
}
