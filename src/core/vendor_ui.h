#ifndef ROGUE_VENDOR_UI_H
#define ROGUE_VENDOR_UI_H
#ifdef __cplusplus
extern "C" { 
#endif
/* Returns current vendor restock progress fraction in [0,1]. */
float rogue_vendor_restock_fraction(void);
#ifdef __cplusplus
}
#endif
#endif
