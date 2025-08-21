#include "core/vendor/vendor_ui.h"
#include <stdio.h>
#include <stdlib.h>

/* Declare bucket helper (implemented in vendor_ui.c) */
int rogue_durability_bucket(float pct);

int main(void)
{
    struct
    {
        float pct;
        int expect;
    } cases[] = {{1.00f, 2}, {0.75f, 2}, {0.59f, 1},  {0.30f, 1},
                 {0.29f, 0}, {0.00f, 0}, {-0.10f, 0}, {1.10f, 2}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int got = rogue_durability_bucket(cases[i].pct);
        if (got != cases[i].expect)
        {
            fprintf(stderr, "DUR_T_FAIL pct=%.2f got=%d expect=%d\n", cases[i].pct, got,
                    cases[i].expect);
            return 10 + (int) i;
        }
    }
    printf("DUR_T_OK buckets validated\n");
    return 0;
}
