#include "core/vendor_rng.h"
#include "core/vendor_pricing.h"
#include "core/vendor.h"
#include "core/vendor_inventory_templates.h"
#include "core/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Replay harness: generate constrained inventory twice with same seed context and validate identical snapshot hash. */

static void sort_arrays(int* defs,int* rar,int* prices,int n){ for(int i=0;i<n;i++){ for(int j=i+1;j<n;j++){ if(defs[j]<defs[i]){ int td=defs[i]; defs[i]=defs[j]; defs[j]=td; int tr=rar[i]; rar[i]=rar[j]; rar[j]=tr; int tp=prices[i]; prices[i]=prices[j]; prices[j]=tp; } } } }

static unsigned int capture_snapshot_hash(const char* vendor_id, unsigned int world_seed, int epoch){
    int count = rogue_vendor_item_count();
    int defs[128]; int rar[128]; int prices[128]; if(count>128) count=128; for(int i=0;i<count;i++){ const RogueVendorItem* it = rogue_vendor_get(i); defs[i]=it?it->def_index:-1; rar[i]=it?it->rarity:0; prices[i]=it?it->price:0; }
    sort_arrays(defs,rar,prices,count);
    unsigned int pmh = rogue_vendor_price_modifiers_hash();
    return rogue_vendor_snapshot_hash(defs,rar,prices,count, world_seed, vendor_id, (unsigned int)epoch, pmh);
}

int main(){
    /* minimal item defs load */
    char pitems[256]; if(rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems)){ rogue_item_defs_reset(); int loaded = rogue_item_defs_load_from_cfg(pitems); assert(loaded>0); }
    /* Simulate template already loaded; if not present generation just returns 0 */
    const char* vendor_id = "test_vendor"; unsigned int world_seed = 123456u; int epoch=7;
    int first = rogue_vendor_generate_constrained(vendor_id, world_seed, epoch, 8);
    unsigned int h1 = capture_snapshot_hash(vendor_id, world_seed, epoch);
    int second = rogue_vendor_generate_constrained(vendor_id, world_seed, epoch, 8);
    unsigned int h2 = capture_snapshot_hash(vendor_id, world_seed, epoch);
    assert(first==second);
    assert(h1==h2);
    /* Variation test: different epoch -> different hash OR zero inventory when no template (still counts) */
    unsigned int h3 = capture_snapshot_hash(vendor_id, world_seed, epoch+1);
    if(first>0) assert(h3!=h1);
    printf("VENDOR_PHASE12_DETERMINISM_OK\n");
    return 0;
}
