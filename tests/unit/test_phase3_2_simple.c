#include "core/integration/combat_equip_bridge.h"
#include <stdio.h>
#include <stdlib.h>

/* Simple test to verify linking */
int main(void)
{
    printf("Phase 3.2 Combat-Equipment Bridge Simple Test\n");

    RogueCombatEquipBridge bridge = {0};
    int result = rogue_combat_equip_bridge_init(&bridge);

    if (result)
    {
        printf("✓ Bridge initialization successful\n");
        rogue_combat_equip_bridge_shutdown(&bridge);
        printf("✓ Bridge shutdown successful\n");
        printf("✓ Phase 3.2 simple test PASSED\n");
        return 0;
    }
    else
    {
        printf("✗ Bridge initialization failed\n");
        return 1;
    }
}
