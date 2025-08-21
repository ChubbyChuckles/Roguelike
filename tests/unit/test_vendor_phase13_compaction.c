#include "core/vendor/vendor_tx_journal.h"
#include <assert.h>
#include <stdio.h>

void test_vendor_phase13_compaction_summary(void) {
    rogue_vendor_tx_journal_reset();
    // Add 3 sales, 2 buybacks, 1 assimilate
    rogue_vendor_tx_journal_append(100, 1, 1, 0xABCDEF01u, 50, 5, 10); // sale
    rogue_vendor_tx_journal_append(110, 1, 1, 0xABCDEF02u, 60, 6, 0); // sale
    rogue_vendor_tx_journal_append(120, 1, 1, 0xABCDEF03u, 70, 7, 0); // sale
    rogue_vendor_tx_journal_append(130, 1, 2, 0xABCDEF04u, 40, 2, 0); // buyback
    rogue_vendor_tx_journal_append(140, 1, 2, 0xABCDEF05u, 30, 3, 0); // buyback
    rogue_vendor_tx_journal_append(150, 1, 3, 0xABCDEF06u, 0, 1, 0);  // assimilate

    RogueVendorTxCompactionSummary summary;
    int rc = rogue_vendor_tx_journal_compact_summary(&summary);
    assert(rc == 0);
    assert(summary.total_sales == 3);
    assert(summary.total_buybacks == 2);
    assert(summary.total_assimilated == 1);
    assert(summary.total_gold_sold == 50+60+70);
    assert(summary.total_gold_bought == 40+30);
    assert(summary.total_rep_delta == 5+6+7+2+3+1);
    assert(summary.first_timestamp_ms == 100);
    assert(summary.last_timestamp_ms == 150);
    printf("[Phase 13] Compaction summary test passed.\n");
}

int main(void) {
    test_vendor_phase13_compaction_summary();
    return 0;
}
