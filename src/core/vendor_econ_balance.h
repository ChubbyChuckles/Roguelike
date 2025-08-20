/* Phase 10 Multi-Vendor Economy Balancing */
#ifndef ROGUE_VENDOR_ECON_BALANCE_H
#define ROGUE_VENDOR_ECON_BALANCE_H
#ifdef __cplusplus
extern "C" { 
#endif

void rogue_vendor_econ_balance_reset(void);
float rogue_vendor_econ_balance_note_price(int price);
float rogue_vendor_inflation_index(void);
float rogue_vendor_dynamic_margin_scalar(void);
float rogue_vendor_biome_scalar(const char* biome_tags);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_VENDOR_ECON_BALANCE_H */
