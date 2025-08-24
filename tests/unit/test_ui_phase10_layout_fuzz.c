#include "../../src/ui/core/ui_test_harness.h"
#include <assert.h>
#include <stdio.h>
int main()
{
    int v = rogue_ui_layout_fuzz(50);
    if (v != 0)
    {
        printf("LAYOUT_FUZZ_FAIL violations=%d\n", v);
        return 1;
    }
    printf("LAYOUT_FUZZ_OK\n");
    return 0;
}
