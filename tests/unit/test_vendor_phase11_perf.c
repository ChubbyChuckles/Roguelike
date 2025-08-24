#include "../../src/core/vendor/vendor_perf.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    rogue_vendor_perf_reset();
    rogue_vendor_perf_init(50);
    assert(rogue_vendor_perf_vendor_count() == 50);
    size_t mem = rogue_vendor_perf_memory_bytes();
    assert(mem < 16000); /* target <16KB for 50 vendors */
    /* Simulate sales and buybacks */
    for (int i = 0; i < 50; i++)
    {
        if (i % 2 == 0)
            rogue_vendor_perf_note_sale(i);
        else
            rogue_vendor_perf_note_buyback(i);
    }
    /* Run scheduler over several ticks to ensure coverage */
    rogue_vendor_perf_scheduler_config(10);
    int total = 0;
    for (int t = 0; t < 6; t++)
    {
        total += rogue_vendor_perf_scheduler_tick(t);
    }
    assert(total >= 50); /* all vendors processed at least once */
    int seen = 0;
    for (int i = 0; i < 50; i++)
    {
        int lt = rogue_vendor_perf_last_refresh_tick(i);
        if (lt >= 0)
            seen++;
    }
    assert(seen == 50);
    printf("VENDOR_PHASE11_PERF_OK\n");
    return 0;
}
