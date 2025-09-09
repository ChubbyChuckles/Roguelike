/* Phase 5: Vendor Transaction Journal (hash chained) */
#ifndef ROGUE_VENDOR_TX_JOURNAL_H
#define ROGUE_VENDOR_TX_JOURNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ROGUE_VENDOR_TX_JOURNAL_CAP
#define ROGUE_VENDOR_TX_JOURNAL_CAP 4096
#endif

    typedef struct RogueVendorTxEntry
    {
        unsigned int op_id;        /* sequence */
        unsigned int timestamp_ms; /* game time snapshot */
        unsigned int vendor_id;    /* hashed vendor def index */
        unsigned int
            action_code; /* 1 sale (player -> vendor), 2 buyback (vendor -> player), 3 assimilate */
        unsigned int item_guid_low; /* low 32 bits for compactness */
        unsigned int price;         /* price transacted */
        unsigned int rep_delta;     /* reputation delta applied */
        unsigned int discount_pct;  /* negotiation discount if any (vendor -> player) */
    } RogueVendorTxEntry;

    /* Phase 13: Journal Compaction & Summary Aggregation */
    typedef struct RogueVendorTxCompactionSummary
    {
        unsigned int total_sales;
        unsigned int total_buybacks;
        unsigned int total_assimilated;
        unsigned int total_gold_sold;
        unsigned int total_gold_bought;
        unsigned int total_rep_delta;
        unsigned int first_timestamp_ms;
        unsigned int last_timestamp_ms;
    } RogueVendorTxCompactionSummary;

    /* Aggregates a summary from the current journal entries. Returns 0 on success, -1 if journal
     * empty. */
    int rogue_vendor_tx_journal_compact_summary(RogueVendorTxCompactionSummary* out_summary);

    void rogue_vendor_tx_journal_reset(void);
    int rogue_vendor_tx_journal_append(unsigned int timestamp_ms, int vendor_def_index,
                                       int action_code, unsigned long long item_guid,
                                       unsigned int price, int rep_delta, int discount_pct);
    int rogue_vendor_tx_journal_count(void);
    const RogueVendorTxEntry* rogue_vendor_tx_journal_entry(int index);
    unsigned int rogue_vendor_tx_journal_accum_hash(void);

    /* Convenience record function used by buyback module */
    void rogue_vendor_tx_journal_record(int vendor_def_index, unsigned long long item_guid,
                                        int action_code, int price, int rep_delta,
                                        int discount_pct);

    /* Phase 15.4: Export/Import for persistence + verification */
    /* Copies up to 'cap' entries into out (may be NULL to query required count). Returns count
     * available (may be >cap); never negative. */
    int rogue_vendor_tx_journal_export_copy(struct RogueVendorTxEntry* out, int cap);
    /* Resets and imports 'count' entries, recomputes the rolling hash and compares against
     * expect_hash. Returns 0 on success (hash match), negative on error or mismatch. */
    int rogue_vendor_tx_journal_import_verify(const struct RogueVendorTxEntry* arr, int count,
                                              unsigned int expect_hash);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VENDOR_TX_JOURNAL_H */
