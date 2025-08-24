#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>

/* Enhanced harness: validates parser ingestion of socket_min/max columns, deterministic fixed
 * count, range bounds, and insert/remove semantics. */
int main(void)
{
    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_cfg("assets/equipment_test_sockets.cfg");
    if (added <= 0)
    {
        added = rogue_item_defs_load_from_cfg("../assets/equipment_test_sockets.cfg");
    }
    assert(added == 2);
    int parse_index = rogue_item_def_index("sock_parse");
    int fixed_index = rogue_item_def_index("sock_fixed");
    assert(parse_index >= 0 && fixed_index >= 0);
    const RogueItemDef* d_parse = rogue_item_def_at(parse_index);
    const RogueItemDef* d_fixed = rogue_item_def_at(fixed_index);
    assert(d_parse && d_parse->socket_min == 2 && d_parse->socket_max == 4);
    assert(d_fixed && d_fixed->socket_min == 3 && d_fixed->socket_max == 3);
    /* Spawn multiple instances of range item at varying positions to exercise LCG variance inside
     * [2,4]. */
    int seen[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < 32; i++)
    {
        int inst = rogue_items_spawn(parse_index, 1, (float) i, 0.f);
        assert(inst >= 0);
        int sc = rogue_item_instance_socket_count(inst);
        assert(sc >= 2 && sc <= 4);
        seen[sc] = 1;
    }
    assert(seen[2] || seen[3] || seen[4]); /* at least one realized */
    /* Spawn fixed count item and verify always 3 sockets. */
    for (int i = 0; i < 5; i++)
    {
        int inst = rogue_items_spawn(fixed_index, 1, (float) i, 1.f);
        assert(inst >= 0);
        int sc = rogue_item_instance_socket_count(inst);
        assert(sc == 3);
        /* Basic insert/remove cycle first socket */
        assert(rogue_item_instance_socket_insert(inst, 0, parse_index) == 0);
        assert(rogue_item_instance_get_socket(inst, 0) == parse_index);
        assert(rogue_item_instance_socket_insert(inst, 0, parse_index) == -3); /* occupied */
        assert(rogue_item_instance_socket_remove(inst, 0) == 0);
        assert(rogue_item_instance_get_socket(inst, 0) == -1);
    }
    printf("equipment_phase5_sockets_ok\n");
    return 0;
}
