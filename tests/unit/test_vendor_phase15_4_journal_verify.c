#include "../../src/core/persistence/save_internal.h"
#include "../../src/core/vendor/vendor_tx_journal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void test_journal_export_import_verify_ok(void)
{
    rogue_vendor_tx_journal_reset();
    unsigned long long base = 0xABCDEF000ULL;
    for (int i = 0; i < 16; ++i)
    {
        rogue_vendor_tx_journal_record(1, base + (unsigned long long) i, (i % 3) + 1, 100 + i,
                                       (i % 2), (i % 4));
    }
    unsigned int h = rogue_vendor_tx_journal_accum_hash();
    int n = rogue_vendor_tx_journal_export_copy(NULL, 0);
    RogueVendorTxEntry* tmp = (RogueVendorTxEntry*) malloc((size_t) n * sizeof *tmp);
    assert(tmp);
    int m = rogue_vendor_tx_journal_export_copy(tmp, n);
    assert(m == n);
    /* Reset and import */
    rogue_vendor_tx_journal_reset();
    assert(rogue_vendor_tx_journal_import_verify(tmp, n, h) == 0);
    free(tmp);
}

static void test_journal_import_mismatch_fails(void)
{
    rogue_vendor_tx_journal_reset();
    unsigned long long base = 0x10ULL;
    for (int i = 0; i < 4; ++i)
        rogue_vendor_tx_journal_record(2, base + (unsigned long long) i, 1, 50 + i, 0, 0);
    unsigned int h = rogue_vendor_tx_journal_accum_hash();
    int n = rogue_vendor_tx_journal_export_copy(NULL, 0);
    RogueVendorTxEntry* tmp = (RogueVendorTxEntry*) malloc((size_t) n * sizeof *tmp);
    assert(tmp);
    rogue_vendor_tx_journal_export_copy(tmp, n);
    /* Tamper */
    if (n > 0)
        tmp[0].price ^= 0x1u;
    rogue_vendor_tx_journal_reset();
    assert(rogue_vendor_tx_journal_import_verify(tmp, n, h) != 0);
    free(tmp);
}

int main(void)
{
    test_journal_export_import_verify_ok();
    test_journal_import_mismatch_fails();
    printf("VENDOR_PHASE15_4_JOURNAL_VERIFY_OK\n");
    return 0;
}
