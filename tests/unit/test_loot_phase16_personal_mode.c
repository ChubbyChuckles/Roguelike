#include "core/loot/loot_instances.h"
#include "../../src/core/loot/loot_multiplayer.h"
#include "core/app_state.h"
#include <stdio.h>
#include <string.h>

static int fail = 0;
#define CHECK(c,msg) do { \
	if(!(c)) { \
		printf("FAIL:%s line %d %s\n", __FILE__, __LINE__, msg); \
		fail = 1; \
	} \
} while(0)

int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	rogue_items_init_runtime();

	/* spawn shared loot first (mode default shared) */
	int a = rogue_items_spawn(0, 1, 0, 0);
	CHECK(a >= 0, "spawn_a");

	/* enable personal mode and spawn second item, then manually assign owner player 1 */
	rogue_loot_set_mode(ROGUE_LOOT_MODE_PERSONAL);
	int b = rogue_items_spawn(0, 2, 0, 0);
	CHECK(b >= 0, "spawn_b");
	g_app.item_instances[b].owner_player_id = -1; /* ensure unowned before assignment */
	rogue_loot_assign_owner(b, 1);

	/* simulate pickup loop: local player id implicitly 0, so item b should remain active */
	extern void rogue_loot_pickup_update(float);
	rogue_loot_pickup_update(1.0f);
	CHECK(g_app.item_instances[a].active == 0, "shared_picked");
	CHECK(g_app.item_instances[b].active == 1, "personal_not_picked");

	/* trade item b to player 0 and confirm pickup */
	CHECK(rogue_loot_trade_request(b, 1, 0) == 0, "trade");
	rogue_loot_pickup_update(1.0f);
	CHECK(g_app.item_instances[b].active == 0, "picked_after_trade");

	if (fail) {
		printf("FAILURES\n");
		return 1;
	}
	printf("OK:loot_phase16_personal_mode\n");
	return 0;
}
