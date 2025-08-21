/* Vendor System Phase 2.1/2.2 Tests
   - Inventory template loading (archetype -> category & rarity weights)
   - Deterministic seed composition (world_seed ^ hash(vendor_id) ^ day_cycle)
*/
#include "core/vendor/vendor_registry.h"
#include "core/vendor/vendor_inventory_templates.h"
#include <stdio.h>
#include <string.h>

static int validate_template(const RogueVendorInventoryTemplate* t){
    if(!t){ printf("VENDOR_P2_FAIL null template\n"); return 0; }
    int cat_sum=0; for(int i=0;i<ROGUE_ITEM__COUNT;i++){ if(t->category_weights[i]<0){ printf("VENDOR_P2_FAIL neg cat weight idx=%d\n",i); return 0; } cat_sum += t->category_weights[i]; }
    if(cat_sum<=0){ printf("VENDOR_P2_FAIL zero cat sum\n"); return 0; }
    int rar_sum=0; for(int r=0;r<5;r++){ if(t->rarity_weights[r]<0){ printf("VENDOR_P2_FAIL neg rarity weight r=%d\n",r); return 0; } rar_sum += t->rarity_weights[r]; }
    if(rar_sum<=0){ printf("VENDOR_P2_FAIL zero rarity sum\n"); return 0; }
    return 1;
}

static int test_seed_determinism(void){
    unsigned int a = rogue_vendor_inventory_seed(12345u, "blacksmith_standard", 7);
    unsigned int b = rogue_vendor_inventory_seed(12345u, "blacksmith_standard", 7);
    if(a!=b){ printf("VENDOR_P2_FAIL seed nondeterministic %u %u\n",a,b); return 0; }
    unsigned int c = rogue_vendor_inventory_seed(12345u, "blacksmith_standard", 8);
    if(c==a){ printf("VENDOR_P2_FAIL day cycle no effect (a=%u c=%u)\n",a,c); return 0; }
    unsigned int d = rogue_vendor_inventory_seed(12345u, "other_vendor", 7);
    if(d==a){ printf("VENDOR_P2_FAIL vendor id no effect (a=%u d=%u)\n",a,d); return 0; }
    return 1;
}

int main(void){
    if(!rogue_vendor_registry_load_all()){ printf("VENDOR_P2_FAIL registry load\n"); return 1; }
    if(!rogue_vendor_inventory_templates_load()){ printf("VENDOR_P2_FAIL template load\n"); return 2; }
    if(rogue_vendor_inventory_template_count()<=0){ printf("VENDOR_P2_FAIL template count\n"); return 3; }
    /* Use archetype from vendors.json: blacksmith_standard vendor has archetype 'blacksmith' */
    const RogueVendorDef* v = rogue_vendor_def_find("blacksmith_standard"); if(!v){ printf("VENDOR_P2_FAIL find vendor\n"); return 4; }
    const RogueVendorInventoryTemplate* t = rogue_vendor_inventory_template_find(v->archetype); if(!validate_template(t)) return 5;
    if(!test_seed_determinism()) return 6;
    printf("VENDOR_PHASE2_INVENTORY_OK templates=%d archetype=%s\n", rogue_vendor_inventory_template_count(), v->archetype);
    return 0;
}
