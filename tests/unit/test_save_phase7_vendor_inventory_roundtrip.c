#include "core/save_manager.h"
#include "core/vendor/vendor.h"
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_tables.h"
#include "core/path_utils.h"
#include "../../src/core/loot/loot_drop_rates.h"
#include <stdio.h>
#include <string.h>

static int fail = 0;
#define CHECK(c,msg) do { \
	if(!(c)) { printf("FAIL:%s %d %s\n", __FILE__, __LINE__, msg); fail = 1; } \
} while(0)

int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	rogue_save_manager_reset_for_tests();
	rogue_save_manager_init();
	extern void rogue_register_core_save_components(void);
	rogue_register_core_save_components();

	/* load test item + loot table data */
	char pitems[256]; char ptables[256];
	if(!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems)) { printf("FAIL:find items\n"); return 2; }
	if(!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables)) { printf("FAIL:find tables\n"); return 3; }
	rogue_item_defs_reset(); int items_loaded = rogue_item_defs_load_from_cfg(pitems); if(items_loaded <= 0) { printf("FAIL:load items\n"); return 4; }
	rogue_drop_rates_reset();
	rogue_loot_tables_reset(); int tables_loaded = rogue_loot_tables_load_from_cfg(ptables); if(tables_loaded <= 0) { printf("FAIL:load tables\n"); return 5; }
	int table_index = rogue_loot_table_index("SKELETON_WARRIOR"); if(table_index < 0) { printf("FAIL:table index\n"); return 6; }

	/* fabricate vendor inventory */
	rogue_vendor_reset();
	unsigned int seed = 12345u;
	RogueGenerationContext ctx = {0};
	ctx.enemy_level = 1;
	int produced = rogue_vendor_generate_inventory(table_index, 4, &ctx, &seed);
	if(produced <= 0) { printf("FAIL:generate produced=%d\n", produced); return 1; }

	/* capture before */
	int before = rogue_vendor_item_count();
	if(rogue_save_manager_save_slot(0) != 0) { printf("FAIL:save\n"); return 1; }

	/* wipe vendor (reset) */
	rogue_vendor_reset();
	if(rogue_vendor_item_count() != 0) { printf("FAIL:wipe\n"); return 1; }

	if(rogue_save_manager_load_slot(0) != 0) { printf("FAIL:load\n"); return 1; }
	int after = rogue_vendor_item_count();
	CHECK(after == before, "count");
	for(int i = 0; i < after; i++) {
		const RogueVendorItem* vit = rogue_vendor_get(i);
		CHECK(vit && vit->def_index >= 0, "entry");
	}
	if(fail) { printf("FAILURES\n"); return 1; }
	printf("OK:save_phase7_vendor_inventory_roundtrip\n");
	return 0;
}
