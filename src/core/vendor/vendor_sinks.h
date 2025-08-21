/* Vendor System Phase 7: Currency & Sink Mechanics */
#ifndef ROGUE_VENDOR_SINKS_H
#define ROGUE_VENDOR_SINKS_H
#ifdef __cplusplus
extern "C"
{
#endif

    /* Sink categories cumulative tracking */
    enum RogueVendorSinkCategory
    {
        ROGUE_SINK_REPAIR = 0,
        ROGUE_SINK_UPGRADE,
        ROGUE_SINK_TRADEIN,
        ROGUE_SINK_FEES,
        ROGUE_SINK__COUNT
    };

    void rogue_vendor_sinks_reset(void);
    void rogue_vendor_sinks_add(int category, int amount);
    int rogue_vendor_sinks_total(int category); /* returns 0 if invalid */
    int rogue_vendor_sinks_grand_total(void);

    /* Upgrade service: reroll a single affix (prefix or suffix) using catalyst + gold fee.
       Parameters:
         inst_index: item instance index
         reroll_prefix: 1 to reroll prefix if present
         reroll_suffix: 1 to reroll suffix if present
         player_level: for fee scaling
         catalyst_count_available: how many catalyst materials player possesses (will consume 1 on
       success if needed) consume_catalyst_cb: callback to spend one catalyst (must return 0 on
       success) spend_gold_cb: callback to spend computed gold fee (0 success, non-zero insufficient
       funds) Returns: 0 on success, negative on error: -1 invalid params -2 nothing selected to
       reroll or affix missing -3 insufficient catalyst -4 gold spend declined -5 reroll operation
       failed (downstream)
    */
    typedef int (*RogueSpendGoldFn)(int amount);
    typedef int (*RogueConsumeCatalystFn)(void);
    int rogue_vendor_upgrade_reroll_affix(int inst_index, int reroll_prefix, int reroll_suffix,
                                          int player_level, int catalyst_count_available,
                                          RogueConsumeCatalystFn consume_catalyst_cb,
                                          RogueSpendGoldFn spend_gold_cb, int* out_gold_fee);

    /* Material trade-in: convert N low-tier materials into 1 higher-tier (unfavorable rate).
       Parameters:
         from_material_index: source material registry index (tier T)
         to_material_index: target material registry index (tier T+1 expected but not enforced
       strictly) count_in: number of source materials offered player_level: for fee scaling (adds
       gold fee proportional) consume_source_cb(count): consumes source materials (must return 0 on
       success) grant_target_cb(count): grants target materials (return 0 on success)
         spend_gold_cb(amount): spend gold fee
       Returns 0 success else negative codes:
         -1 invalid params
         -2 count_in below minimum (rate divisor)
         -3 consume failed
         -4 gold spend failed
         -5 grant failed
    */
    typedef int (*RogueConsumeSourceMatsFn)(int count);
    typedef int (*RogueGrantTargetMatsFn)(int count);
    int rogue_vendor_material_trade_in(int from_material_index, int to_material_index, int count_in,
                                       int player_level, RogueConsumeSourceMatsFn consume_source_cb,
                                       RogueGrantTargetMatsFn grant_target_cb,
                                       RogueSpendGoldFn spend_gold_cb, int* out_count_out,
                                       int* out_gold_fee);

#ifdef __cplusplus
}
#endif
#endif
