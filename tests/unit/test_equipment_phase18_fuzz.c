/* Phase 18.2: Fuzz equip/un-equip sequences test */
#include "core/equipment/equipment_fuzz.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int v = rogue_equipment_fuzz_sequences(500, 1337);
    if (v != 0)
    {
        printf("FAIL:fuzz violations=%d\n", v);
        return 1;
    }
    printf("Phase18.2 fuzz OK\n");
    return 0;
}
