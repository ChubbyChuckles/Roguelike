#include "core/vendor/vendor_buyback.h"
#include "core/vendor/vendor_tx_journal.h"
#include <assert.h>
#include <stdio.h>

/* Lightweight fake time progression */
static unsigned int now_ms = 0;
static void advance(unsigned int ms) { now_ms += ms; }

static void test_ring_buffer_wrap(void)
{
    rogue_vendor_buyback_reset();
    int vendor = 0;
    now_ms = 0;
    for (int i = 0; i < ROGUE_VENDOR_BUYBACK_CAP + 5; i++)
    {
        unsigned long long guid = 0xABC00000ULL + (unsigned long long) i;
        int slot = rogue_vendor_buyback_record(vendor, guid, 1, 0, 0, 100, 50 + i, now_ms);
        assert(slot >= 0);
        advance(10);
    }
    RogueVendorBuybackEntry tmp[ROGUE_VENDOR_BUYBACK_CAP];
    int count = rogue_vendor_buyback_list(vendor, tmp, ROGUE_VENDOR_BUYBACK_CAP, now_ms);
    assert(count <= ROGUE_VENDOR_BUYBACK_CAP);
}

static void test_depreciation_monotonic(void)
{
    rogue_vendor_buyback_reset();
    now_ms = 0;
    int vendor = 1;
    unsigned long long guid = 0x12345678ULL;
    rogue_vendor_buyback_record(vendor, guid, 1, 0, 0, 100, 100, now_ms);
    RogueVendorBuybackEntry list[4];
    int before = rogue_vendor_buyback_list(vendor, list, 4, now_ms);
    assert(before == 1);
    int p0 = rogue_vendor_buyback_current_price(&list[0], now_ms);
    advance(61 * 1000); /* >1 interval */
    rogue_vendor_buyback_list(vendor, list, 4, now_ms);
    int p1 = rogue_vendor_buyback_current_price(&list[0], now_ms);
    advance(61 * 1000);
    rogue_vendor_buyback_list(vendor, list, 4, now_ms);
    int p2 = rogue_vendor_buyback_current_price(&list[0], now_ms);
    assert(p0 >= p1 && p1 >= p2);
    assert(p2 >= (int) (100 * 0.5f)); /* floor 50% */
}

static void test_journal_hash_determinism(void)
{
    rogue_vendor_tx_journal_reset();
    now_ms = 0;
    unsigned long long g = 0x9999ULL;
    for (int i = 0; i < 10; i++)
    {
        rogue_vendor_tx_journal_record(0, g + i, (i % 2) + 1, 100 + i, 0, 0);
    }
    unsigned int hash1 = rogue_vendor_tx_journal_accum_hash();
    rogue_vendor_tx_journal_reset();
    for (int i = 0; i < 10; i++)
    {
        rogue_vendor_tx_journal_record(0, g + i, (i % 2) + 1, 100 + i, 0, 0);
    }
    unsigned int hash2 = rogue_vendor_tx_journal_accum_hash();
    assert(hash1 == hash2);
}

static void test_duplicate_guid_detection(void)
{
    rogue_vendor_buyback_reset();
    now_ms = 0;
    int vendor = 2;
    unsigned long long guid = 0xDEADBEEFULL;
    rogue_vendor_buyback_record(vendor, guid, 1, 0, 0, 100, 50, now_ms);
    int recent1 = rogue_vendor_buyback_guid_recent(guid);
    /* recent API notes only via _with_journal path but we simulate manual note by recording again
     */
    rogue_vendor_buyback_record(vendor, guid, 1, 0, 0, 100, 50, now_ms + 1);
    int recent2 = rogue_vendor_buyback_guid_recent(guid);
    /* first call might be 0 (not yet noted), second should be 1 (duplicate suggests exploit) */
    assert(recent2 == 1);
    (void) recent1; /* silence if unused depending on first semantics */
}

int main()
{
    test_ring_buffer_wrap();
    test_depreciation_monotonic();
    test_journal_hash_determinism();
    test_duplicate_guid_detection();
    printf("VENDOR_PHASE5_BUYBACK_JOURNAL_OK\n");
    return 0;
}
