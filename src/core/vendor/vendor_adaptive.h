/* Phase 9 Adaptive Vendor Behavior (purchase profile, preference adaptation, flip detection, batch
 * gating) */
#ifndef ROGUE_VENDOR_ADAPTIVE_H
#define ROGUE_VENDOR_ADAPTIVE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    void rogue_vendor_adaptive_reset(void);
    void rogue_vendor_adaptive_record_player_purchase(int category, uint32_t timestamp_ms);
    void rogue_vendor_adaptive_record_player_sale(int category, uint32_t timestamp_ms);
    float rogue_vendor_adaptive_category_weight_scalar(int category);
    void rogue_vendor_adaptive_apply_category_weights(int* out_weights, const int* base_weights,
                                                      int count);
    float rogue_vendor_adaptive_exploit_scalar(void);
    uint32_t rogue_vendor_adaptive_purchase_cooldown_remaining(uint32_t now_ms);
    int rogue_vendor_adaptive_can_purchase(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VENDOR_ADAPTIVE_H */
